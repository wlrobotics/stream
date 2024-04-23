#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace tool {
    template<typename T>
    class BlockingQueue {
    public:
        BlockingQueue() = default;
        ~BlockingQueue() = default;

        bool init(int max_queue_size) {
            max_queue_size_ = max_queue_size;
            return true;
        }

        void push(T& pkt) {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            if(queue_.size() >= max_queue_size_) {
                queue_= std::queue<T>();
            }
            queue_.push(pkt);
            not_empty_.notify_one();
        }

         T pop() {
            std::unique_lock<std::mutex> locker(queue_mutex_);
            not_empty_.wait(locker, [this]{return !queue_.empty();});
            T pkt = queue_.front();
            queue_.pop();
            return std::move(pkt);
        }

    private:
        unsigned int max_queue_size_ = 0;
        std::queue<T> queue_;
        std::condition_variable not_empty_;
        std::mutex queue_mutex_;
    };
}
