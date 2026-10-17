#include "oma/base/bounded_queue.hpp"

#include <atomic>
#include <chrono>
#include <thread>

#include "oma_test.hpp"

// pop(): a consumer that waits for items instead of polling (the export's encoder stage).

using oma::BoundedQueue;

namespace {

// A waiting consumer receives 0..n-1 in order, then nothing once the producer closes.
bool pop_receives_in_order_then_ends(int n) {
    BoundedQueue<int> q(2);
    std::thread producer([&] {
        for (int i = 0; i < n; ++i) {
            q.push(i);
        }
        q.close();
    });
    int expected = 0;
    bool ordered = true;
    while (auto v = q.pop()) {
        ordered = ordered && *v == expected;
        ++expected;
    }
    producer.join();
    return ordered && expected == n;
}

// close() wakes a consumer waiting on an empty queue.
bool close_releases_consumer() {
    BoundedQueue<int> q(1);
    std::atomic<int> got{-1};
    std::thread consumer([&] { got.store(q.pop() ? 1 : 0); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    q.close();
    consumer.join();
    return got.load() == 0;
}

} // namespace

void run_bounded_queue_pop_tests() {
    describe("BoundedQueue::pop", {
        it("waits for items in order and ends after close",
           { expect(pop_receives_in_order_then_ends(10000)).toBeTruthy(); });
        it("releases a waiting consumer on close",
           { expect(close_releases_consumer()).toBeTruthy(); });
    });
}
