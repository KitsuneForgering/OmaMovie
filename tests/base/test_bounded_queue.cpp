#include "oma/base/bounded_queue.hpp"

#include <atomic>
#include <chrono>
#include <thread>

#include "oma_test.hpp"

using oma::BoundedQueue;

namespace {

// A producer pushing 0..n-1 through a small queue while the consumer polls; returns the first
// out-of-order value, or -1.
long long stream_in_order(int n) {
    BoundedQueue<int> q(3);
    std::thread producer([&] {
        for (int i = 0; i < n; ++i) {
            q.push(i);
        }
    });
    long long mismatch = -1;
    int expected = 0;
    while (expected < n) {
        if (auto v = q.try_pop()) {
            if (*v != expected && mismatch < 0) {
                mismatch = expected;
            }
            ++expected;
        } else {
            std::this_thread::yield();
        }
    }
    producer.join();
    return mismatch;
}

// Whether a producer blocked on a full queue stays blocked until there is room.
bool push_waits_for_room() {
    BoundedQueue<int> q(1);
    q.push(1);
    std::atomic<bool> pushed{false};
    std::thread producer([&] {
        q.push(2);
        pushed.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool blocked = !pushed.load();
    (void)q.try_pop();
    producer.join();
    return blocked && pushed.load() && q.size() == 1;
}

// Whether close() releases a blocked producer and makes its push fail.
bool close_releases_producer() {
    BoundedQueue<int> q(1);
    q.push(1);
    std::atomic<int> result{-1};
    std::thread producer([&] { result.store(q.push(2) ? 1 : 0); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.close();
    producer.join();
    return result.load() == 0 && q.size() == 1;
}

} // namespace

void run_bounded_queue_tests() {
    describe("BoundedQueue", {
        it("keeps order between a producer and a consumer thread", {
            expect(stream_in_order(5000)).toBe(-1LL);
        });

        it("blocks the producer while full", { expect(push_waits_for_room()).toBeTruthy(); });

        it("releases a blocked producer on close", { expect(close_releases_producer()).toBeTruthy(); });

        it("pops only what the predicate accepts", {
            BoundedQueue<int> q(4);
            q.push(5);
            q.push(6);
            expect(q.try_pop_if([](int v) { return v > 5; }).has_value()).toBeFalsy();
            expect(q.try_pop_if([](int v) { return v == 5; }).value_or(0)).toBe(5);
            expect(q.size()).toBe(1U);
        });

        it("drops everything on clear and keeps accepting", {
            BoundedQueue<int> q(2);
            q.push(1);
            q.push(2);
            q.clear();
            expect(q.size()).toBe(0U);
            expect(q.push(3)).toBeTruthy();
        });

        it("treats a zero capacity as one", {
            BoundedQueue<int> q(0);
            expect(q.capacity()).toBe(1U);
        });
    });
}
