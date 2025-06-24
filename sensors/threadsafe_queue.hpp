#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
    private:
        std::queue<T> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cond_;

    public:
        ThreadSafeQueue() = default;

        void push(const T& item) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.push(item);
            }
            cond_.notify_one();
        }

        bool try_pop(T& item) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) return false;
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void wait_and_pop(T& item) {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this]() { return !queue_.empty(); });
            item = std::move(queue_.front());
            queue_.pop();
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }
};
