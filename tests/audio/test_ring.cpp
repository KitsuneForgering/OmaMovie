#include "oma/audio/ring.hpp"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "oma_test.hpp"

using oma::audio::SampleRing;

namespace {

std::vector<float> ramp(std::size_t count, float first) {
    std::vector<float> v(count);
    for (std::size_t i = 0; i < count; ++i) {
        v[i] = first + static_cast<float>(i);
    }
    return v;
}

// Streams `total` stereo frames (values 0, 1, 2, ... per sample) from a producer thread through a
// small ring and checks the consumer receives them in order. Returns the first mismatch index
// or -1.
long long stream_through_ring(std::size_t total) {
    SampleRing ring(64, 2);
    std::thread producer([&] {
        std::size_t sent = 0;
        while (sent < total) {
            const std::size_t chunk = std::min<std::size_t>(37, total - sent);
            std::vector<float> data(chunk * 2);
            for (std::size_t i = 0; i < data.size(); ++i) {
                data[i] = static_cast<float>((sent * 2) + i);
            }
            sent += ring.write(data);
            std::this_thread::yield();
        }
    });
    long long mismatch = -1;
    std::size_t received = 0;
    std::vector<float> buffer(2 * 23);
    while (received < total) {
        const std::size_t got = ring.read(buffer);
        for (std::size_t i = 0; i < got * 2 && mismatch < 0; ++i) {
            if (buffer[i] != static_cast<float>((received * 2) + i)) {
                mismatch = static_cast<long long>((received * 2) + i);
            }
        }
        received += got;
        std::this_thread::yield();
    }
    producer.join();
    return mismatch;
}

} // namespace

void run_ring_tests() {
    describe("SampleRing", {
        it("rounds the capacity up to a power of two frames", {
            SampleRing ring(100, 2);
            expect(ring.capacity()).toBe(128U);
            expect(ring.writable()).toBe(128U);
            expect(ring.readable()).toBe(0U);
        });

        it("writes and reads whole interleaved frames in order", {
            SampleRing ring(8, 2);
            const auto in = ramp(6, 0.0F);
            expect(ring.write(in)).toBe(3U);
            std::vector<float> out(6);
            expect(ring.read(out)).toBe(3U);
            expect(out[0]).toBe(0.0F);
            expect(out[5]).toBe(5.0F);
        });

        it("ignores a trailing partial frame", {
            SampleRing ring(8, 2);
            const auto in = ramp(5, 0.0F);
            expect(ring.write(in)).toBe(2U);
            expect(ring.readable()).toBe(2U);
        });

        it("accepts only what fits and keeps the rest for later", {
            SampleRing ring(4, 1);
            const auto in = ramp(6, 10.0F);
            expect(ring.write(in)).toBe(4U);
            expect(ring.writable()).toBe(0U);
            std::vector<float> out(2);
            expect(ring.read(out)).toBe(2U);
            expect(out[1]).toBe(11.0F);
            const auto more = ramp(3, 20.0F);
            expect(ring.write(more)).toBe(2U);
            std::vector<float> rest(8);
            expect(ring.read(rest)).toBe(4U);
            expect(rest[0]).toBe(12.0F);
            expect(rest[3]).toBe(21.0F);
        });

        it("drops everything readable on discard", {
            SampleRing ring(8, 2);
            const auto in = ramp(8, 0.0F);
            ring.write(in);
            ring.discard_readable();
            expect(ring.readable()).toBe(0U);
            expect(ring.writable()).toBe(8U);
        });

        it("keeps order between a producer and a consumer thread",
           { expect(stream_through_ring(20000)).toBe(-1LL); });
    });
}
