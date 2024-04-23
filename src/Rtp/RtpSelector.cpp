#include "RtpSelector.h"

#include <stddef.h>

#include "RtpSplitter.h"

namespace mediakit{

INSTANCE_IMP(RtpSelector);

void RtpSelector::clear(){
    lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    _map_rtp_process.clear();
}

bool RtpSelector::inputRtp(const Socket::Ptr &sock,
                              const char *data, int data_len,
                              const struct sockaddr *addr,
                              uint32_t *dts_out) {                    
    uint32_t ssrc = 0;
    if (!getSSRC(data, data_len, ssrc)) {
        WarnL << "get ssrc from rtp failed:" << data_len;
        return false;
    }
    auto process = getProcess(ssrc);
    if (process) {
        try {
            return process->inputRtp(true, sock, data, data_len, addr, dts_out);
        } catch (...) {
            delProcess(process->getIdentifier(), process.get());
            throw;
        }
    }
    return false;
}

bool RtpSelector::getSSRC(const char *data,int data_len, uint32_t &ssrc){
    if (data_len < 12) {
        return false;
    }
    uint32_t *ssrc_ptr = (uint32_t *) (data + 8);
    ssrc = ntohl(*ssrc_ptr);
    return true;
}

RtpProcess::Ptr RtpSelector::getProcess(const std::uint32_t ssrc) {
    const std::string ssrc_str = std::to_string(ssrc);
    std::lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    auto it = _map_rtp_process.find(ssrc_str);
    if (it != _map_rtp_process.end()) {
        return it->second->getProcess();
    }
    
    std::string stream_id;

    RtpProcessHelper::Ptr process = std::make_shared<RtpProcessHelper>(stream_id, shared_from_this());
    process->attachEvent();
    _map_rtp_process[ssrc_str] = process;
    _map_ssrc_streamid[stream_id] = ssrc_str;
    createTimer();
    
    return process->getProcess();
}

RtpProcess::Ptr RtpSelector::getProcess(const std::string& stream_id, bool make_new) {
    std::lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    auto it = _map_rtp_process.find(stream_id);
    if (it != _map_rtp_process.end()) {
        return it->second->getProcess();
    }

    auto ssrc_it = _map_ssrc_streamid.find(stream_id);
    if(ssrc_it != _map_ssrc_streamid.end()) {
        auto it = _map_rtp_process.find(ssrc_it->second);
        if (it != _map_rtp_process.end()) {
            return it->second->getProcess();
        }
    }

    if(!make_new) {
        return nullptr;
    }

    RtpProcessHelper::Ptr process = std::make_shared<RtpProcessHelper>(stream_id, shared_from_this());
    process->attachEvent();
    _map_rtp_process[stream_id] = process;
    createTimer();
    
    return process->getProcess();
}

void RtpSelector::createTimer() {
    if (!_timer) {
        std::weak_ptr<RtpSelector> weakSelf = shared_from_this();
        _timer = std::make_shared<Timer>(3.0f, [weakSelf] {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onManager();
            return true;
        }, EventPollerPool::Instance().getPoller());
    }
}

void RtpSelector::delProcess(const string &stream_id, const RtpProcess *ptr) {
    RtpProcess::Ptr process;
    {
        std::lock_guard<decltype(_mtx_map)> lck(_mtx_map);
        std::string id = stream_id;
        auto ssrc_it = _map_ssrc_streamid.find(stream_id);
        if(ssrc_it != _map_ssrc_streamid.end()) {
            id = ssrc_it->second;
            ssrc_it = _map_ssrc_streamid.erase(ssrc_it);
        }
        InfoL << "delProcess stream_id=" << stream_id;
        auto it = _map_rtp_process.find(id);
        if (it == _map_rtp_process.end()) {
            WarnL << "not found stream_id=" << id;
            return;
        }
        if (it->second->getProcess().get() != ptr) {
            WarnL << "not found stream_id=" << id;
            return;
        }
        process = it->second->getProcess();
        _map_rtp_process.erase(it);
    }
    process->onDetach();
}

void RtpSelector::for_each_process(const function<void(const string &streamid,
                                        const RtpProcess::Ptr _process)> &cb) {
    RtpProcess::Ptr process;
    std::lock_guard<decltype(_mtx_map)> lck(_mtx_map);
    for(auto iter : _map_rtp_process) {
        process = iter.second->getProcess();
        cb(process->getIdentifier(), process);
    }
}

void RtpSelector::onManager() {
    List<RtpProcess::Ptr> clear_list;
    {
        std::lock_guard<decltype(_mtx_map)> lck(_mtx_map);
        for (auto it = _map_rtp_process.begin(); it != _map_rtp_process.end();) {
            if (it->second->getProcess()->alive()) {
                ++it;
                continue;
            }
            std::string stream_id = it->second->getProcess()->getIdentifier();
            ErrorL << "RtpProcess timeout:" << stream_id;
            _map_ssrc_streamid.erase(stream_id);
            clear_list.emplace_back(it->second->getProcess());
            it = _map_rtp_process.erase(it);
        }
    }

    clear_list.for_each([](const RtpProcess::Ptr &process) {
        process->onDetach();
    });
}

RtpSelector::RtpSelector() {
}

RtpSelector::~RtpSelector() {
}

RtpProcessHelper::RtpProcessHelper(const string &stream_id, const weak_ptr<RtpSelector> &parent) {
    _stream_id = stream_id;
    _parent = parent;
    _process = std::make_shared<RtpProcess>(stream_id);
}

RtpProcessHelper::~RtpProcessHelper() {
}

void RtpProcessHelper::attachEvent() {
    _process->setListener(shared_from_this());
}

bool RtpProcessHelper::close(MediaSource &sender, bool force) {
    if (!_process || (!force && _process->totalReaderCount())) {
        return false;
    }
    auto parent = _parent.lock();
    if (!parent) {
        return false;
    }
    parent->delProcess(_stream_id, _process.get());
    WarnL << "close media:" << sender.getSchema() << "/" 
                            << sender.getApp() << "/" 
                            << sender.getId() << " " << force;
    return true;
}

int RtpProcessHelper::totalReaderCount(MediaSource &sender) {
    return _process ? _process->totalReaderCount() : sender.totalReaderCount();
}

RtpProcess::Ptr &RtpProcessHelper::getProcess() {
    return _process;
}

}//namespace mediakit
