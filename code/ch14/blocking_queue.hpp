// A bounded blocking queue: one mutex, two condition variables (Section 14.1).
// The classic interview answer; also the "naive" side of the showdown in 14.10.
#pragma once
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

template <class T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : cap_(capacity) {}

    // Blocks while full. Returns false if the queue was closed.
    bool push(T v) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || closed_; });
        if (closed_) return false;
        q_.push_back(std::move(v));
        lk.unlock();                       // wake the consumer after releasing the lock
        not_empty_.notify_one();
        return true;
    }

    // Blocks while empty. Returns nullopt once closed and drained.
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || closed_; });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop_front();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    void close() {
        {
            std::lock_guard lk(m_);
            closed_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable not_full_, not_empty_;
    std::deque<T> q_;                      // guarded by m_
    std::size_t cap_;
    bool closed_ = false;                  // guarded by m_
};
