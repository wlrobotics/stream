#include "EventPoller.h"
 
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <list>

#include "Util/util.h"
#include "Util/logger.h"
#include "Util/uv_errno.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
#include "Network/sockutil.h"

#if !defined(EPOLLEXCLUSIVE)
#define EPOLLEXCLUSIVE 0
#endif

#define EPOLL_SIZE 1024
#define toEpoll(event)    (((event) & Event_Read) ? EPOLLIN : 0) \
                            | (((event) & Event_Write) ? EPOLLOUT : 0) \
                            | (((event) & Event_Error) ? (EPOLLHUP | EPOLLERR) : 0) \
                            | (((event) & Event_LT) ?  0 : EPOLLET)
                            
#define toPoller(epoll_event) (((epoll_event) & EPOLLIN) ? Event_Read : 0) \
                            | (((epoll_event) & EPOLLOUT) ? Event_Write : 0) \
                            | (((epoll_event) & EPOLLHUP) ? Event_Error : 0) \
                            | (((epoll_event) & EPOLLERR) ? Event_Error : 0)

namespace toolkit {

EventPoller &EventPoller::Instance() {
    return *(EventPollerPool::Instance().getFirstPoller());
}

EventPoller::EventPoller(ThreadPool::Priority priority ) {
    _priority = priority;
    SockUtil::setNoBlocked(_pipe.readFD());
    SockUtil::setNoBlocked(_pipe.writeFD());

    _epoll_fd = epoll_create(EPOLL_SIZE);
    if (_epoll_fd == -1) {
        throw runtime_error(StrPrinter << "创建epoll文件描述符失败:" << get_uv_errmsg());
    }
    SockUtil::setCloExec(_epoll_fd);

    _logger = Logger::Instance().shared_from_this();
    _loop_thread_id = this_thread::get_id();

    //添加内部管道事件
    if (addEvent(_pipe.readFD(), Event_Read, [this](int event) { onPipeEvent(); }) == -1) {
        throw std::runtime_error("epoll添加管道失败");
    }
}

void EventPoller::shutdown() {
    async_l([]() {
        throw ExitException();
    }, false, true);

    if (_loop_thread) {
        //防止作为子进程时崩溃
        try { _loop_thread->join(); } catch (...) { }
        delete _loop_thread;
        _loop_thread = nullptr;
    }
}

EventPoller::~EventPoller() {
    shutdown();
    wait();
    if (_epoll_fd != -1) {
        close(_epoll_fd);
        _epoll_fd = -1;
    }
    //退出前清理管道中的数据
    _loop_thread_id = this_thread::get_id();
    onPipeEvent();
    InfoL << this;
}

int EventPoller::addEvent(int fd, int event, PollEventCB cb) {
    TimeTicker();
    if (!cb) {
        WarnL << "PollEventCB 为空!";
        return -1;
    }

    if (isCurrentThread()) {
        struct epoll_event ev = {0};
        ev.events = (toEpoll(event)) | EPOLLEXCLUSIVE;
        ev.data.fd = fd;
        int ret = epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        if (ret == 0) {
            _event_map.emplace(fd, std::make_shared<PollEventCB>(std::move(cb)));
        }
        return ret;
    }

    async([this, fd, event, cb]() {
        addEvent(fd, event, std::move(const_cast<PollEventCB &>(cb)));
    });
    return 0;
}

int EventPoller::delEvent(int fd, PollDelCB cb) {
    TimeTicker();
    if (!cb) {
        cb = [](bool success) {};
    }

    if (isCurrentThread()) {
        bool success = epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL) == 0 && _event_map.erase(fd) > 0;
        cb(success);
        return success ? 0 : -1;
    }

    //跨线程操作
    async([this, fd, cb]() {
        delEvent(fd, std::move(const_cast<PollDelCB &>(cb)));
    });
    return 0;
}

int EventPoller::modifyEvent(int fd, int event) {
    TimeTicker();
    struct epoll_event ev = {0};
    ev.events = toEpoll(event);
    ev.data.fd = fd;
    return epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

Task::Ptr EventPoller::async(TaskIn task, bool may_sync) {
    return async_l(std::move(task), may_sync, false);
}

Task::Ptr EventPoller::async_first(TaskIn task, bool may_sync) {
    return async_l(std::move(task), may_sync, true);
}

Task::Ptr EventPoller::async_l(TaskIn task,bool may_sync, bool first) {
    TimeTicker();
    if (may_sync && isCurrentThread()) {
        task();
        return nullptr;
    }

    auto ret = std::make_shared<Task>(std::move(task));
    {
        lock_guard<mutex> lck(_mtx_task);
        if (first) {
            _list_task.emplace_front(ret);
        } else {
            _list_task.emplace_back(ret);
        }
    }
    //写数据到管道,唤醒主线程
    _pipe.write("", 1);
    return ret;
}

bool EventPoller::isCurrentThread() {
    return _loop_thread_id == this_thread::get_id();
}

inline void EventPoller::onPipeEvent() {
    TimeTicker();
    char buf[1024];
    int err = 0;
    do {
        if (_pipe.read(buf, sizeof(buf)) > 0) {
            continue;
        }
        err = get_uv_error(true);
    } while (err != UV_EAGAIN);

    decltype(_list_task) _list_swap;
    {
        lock_guard<mutex> lck(_mtx_task);
        _list_swap.swap(_list_task);
    }

    _list_swap.for_each([&](const Task::Ptr &task) {
        try {
            (*task)();
        } catch (ExitException &ex) {
            _exit_flag = true;
        } catch (std::exception &ex) {
            ErrorL << "EventPoller执行异步任务捕获到异常:" << ex.what();
        }
    });
}

void EventPoller::wait() {
    lock_guard<mutex> lck(_mtx_runing);
}

static mutex s_all_poller_mtx;
static map<thread::id, weak_ptr<EventPoller> > s_all_poller;

BufferRaw::Ptr EventPoller::getSharedBuffer() {
    auto ret = _shared_buffer.lock();
    if (!ret) {
        //预留一个字节存放\0结尾符
        ret = std::make_shared<BufferRaw>(1 + SOCKET_DEFAULT_BUF_SIZE);
        _shared_buffer = ret;
    }
    return ret;
}

//static
EventPoller::Ptr EventPoller::getCurrentPoller(){
    lock_guard<mutex> lck(s_all_poller_mtx);
    auto it = s_all_poller.find(this_thread::get_id());
    if (it == s_all_poller.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void EventPoller::runLoop(bool blocked,bool regist_self) {
    if (blocked) {
        ThreadPool::setPriority(_priority);
        lock_guard<mutex> lck(_mtx_runing);
        _loop_thread_id = this_thread::get_id();
        if (regist_self) {
            lock_guard<mutex> lck(s_all_poller_mtx);
            s_all_poller[_loop_thread_id] = shared_from_this();
        }
        _sem_run_started.post();
        _exit_flag = false;
        uint64_t minDelay;
        struct epoll_event events[EPOLL_SIZE];
        while (!_exit_flag) {
            minDelay = getMinDelay();
            startSleep();//用于统计当前线程负载情况
            int ret = epoll_wait(_epoll_fd, events, EPOLL_SIZE, minDelay ? minDelay : -1);
            sleepWakeUp();//用于统计当前线程负载情况
            if (ret <= 0) {
                //超时或被打断
                continue;
            }
            for (int i = 0; i < ret; ++i) {
                struct epoll_event &ev = events[i];
                int fd = ev.data.fd;
                auto it = _event_map.find(fd);
                if (it == _event_map.end()) {
                    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    continue;
                }
                auto cb = it->second;
                try {
                    (*cb)(toPoller(ev.events));
                } catch (std::exception &ex) {
                    ErrorL << "EventPoller执行事件回调捕获到异常:" << ex.what();
                }
            }
        }
    } else {
        _loop_thread = new thread(&EventPoller::runLoop, this, true, regist_self);
        _sem_run_started.wait();
    }
}

uint64_t EventPoller::flushDelayTask(uint64_t now_time) {
    decltype(_delay_task_map) task_copy;
    task_copy.swap(_delay_task_map);

    for (auto it = task_copy.begin(); it != task_copy.end() && it->first <= now_time; it = task_copy.erase(it)) {
        //已到期的任务
        try {
            auto next_delay = (*(it->second))();
            if (next_delay) {
                //可重复任务,更新时间截止线
                _delay_task_map.emplace(next_delay + now_time, std::move(it->second));
            }
        } catch (std::exception &ex) {
            ErrorL << "EventPoller执行延时任务捕获到异常:" << ex.what();
        }
    }

    task_copy.insert(_delay_task_map.begin(), _delay_task_map.end());
    task_copy.swap(_delay_task_map);

    auto it = _delay_task_map.begin();
    if (it == _delay_task_map.end()) {
        //没有剩余的定时器了
        return 0;
    }
    //最近一个定时器的执行延时
    return it->first - now_time;
}

uint64_t EventPoller::getMinDelay() {
    auto it = _delay_task_map.begin();
    if (it == _delay_task_map.end()) {
        //没有剩余的定时器了
        return 0;
    }
    auto now = getCurrentMillisecond();
    if (it->first > now) {
        //所有任务尚未到期
        return it->first - now;
    }
    //执行已到期的任务并刷新休眠延时
    return flushDelayTask(now);
}

DelayTask::Ptr EventPoller::doDelayTask(uint64_t delayMS, function<uint64_t()> task) {
    DelayTask::Ptr ret = std::make_shared<DelayTask>(std::move(task));
    auto time_line = getCurrentMillisecond() + delayMS;
    async_first([time_line, ret, this]() {
        //异步执行的目的是刷新select或epoll的休眠时间
        _delay_task_map.emplace(time_line, ret);
    });
    return ret;
}

///////////////////////////////////////////////

int s_pool_size = 0;

INSTANCE_IMP(EventPollerPool);

EventPoller::Ptr EventPollerPool::getFirstPoller(){
    return dynamic_pointer_cast<EventPoller>(_threads.front());
}

EventPoller::Ptr EventPollerPool::getPoller(){
    auto poller = EventPoller::getCurrentPoller();
    if(_preferCurrentThread && poller){
        return poller;
    }
    return dynamic_pointer_cast<EventPoller>(getExecutor());
}

void EventPollerPool::preferCurrentThread(bool flag){
    _preferCurrentThread = flag;
}

EventPollerPool::EventPollerPool(){
    auto size = s_pool_size > 0 ? s_pool_size : std::thread::hardware_concurrency();
    createThreads([]() {
        EventPoller::Ptr ret(new EventPoller);
        ret->runLoop(false, true);
        return ret;
    }, size);
    InfoL << "EventPoller size=" << size;
}

void EventPollerPool::setPoolSize(int size) {
    s_pool_size = size;
}


}  // namespace toolkit

