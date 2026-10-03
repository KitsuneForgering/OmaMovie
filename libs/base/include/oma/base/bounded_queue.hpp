#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

// A bounded FIFO between pipeline stages (CLAUDE.md §13: queues between stages are bounded,
// giving backpressure). The producer blocks while the queue is full; the consumer never blocks.
// close() releases a blocked producer for cancellation and shutdown.
//
// Not for the real-time audio thread: it takes a mutex. Real-time paths use lock-free rings.

namespace oma {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;
    ~BoundedQueue() = default;

    // Waits for room, then appends. Returns false (dropping `value`) once the queue is closed.
    bool push(T value) {
        std::unique_lock lock(mutex_);
        room_.wait(lock, [&] { return closed_ || items_.size() < capacity_; });
        if (closed_) {
            return false;
        }
        items_.push_back(std::move(value));
        return true;
    }

    // The oldest item, if any.
    std::optional<T> try_pop() {
        return try_pop_if([](const T&) { return true; });
    }

    // The oldest item, only if `take(item)` says so; otherwise it stays queued.
    template <typename Pred>
    std::optional<T> try_pop_if(Pred take) {
        std::optional<T> out;
        {
            const std::scoped_lock lock(mutex_);
            if (items_.empty() || !take(std::as_const(items_.front()))) {
                return out;
            }
            out.emplace(std::move(items_.front()));
            items_.pop_front();
        }
        room_.notify_one();
        return out;
    }

    // Drops everything queued (the producer may continue).
    void clear() {
        {
            const std::scoped_lock lock(mutex_);
            items_.clear();
        }
        room_.notify_all();
    }

    // Wakes a blocked producer; every later push fails. Queued items stay readable.
    void close() {
        {
            const std::scoped_lock lock(mutex_);
            closed_ = true;
        }
        room_.notify_all();
    }

    [[nodiscard]] std::size_t size() const {
        const std::scoped_lock lock(mutex_);
        return items_.size();
    }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable room_;
    std::deque<T> items_;
    bool closed_ = false;
};

} // namespace oma
