#include "oma/audio/clock.hpp"

#include "oma_test.hpp"

using oma::Rational;
using oma::RationalTime;
using oma::SampleRate;
using oma::audio::PlaybackClock;

namespace {

SampleRate hz(int rate) {
    return SampleRate::make(rate).value();
}

RationalTime at(std::int64_t value, std::int64_t num, std::int64_t den) {
    return RationalTime::make(value, Rational::make(num, den).value()).value();
}

} // namespace

void run_clock_tests() {
    describe("PlaybackClock", {
        it("starts at the anchor and advances with consumed frames", {
            PlaybackClock clock(hz(48000));
            expect(clock.anchor(at(0, 1, 1), 0).has_value()).toBeTruthy();
            expect(clock.position(0, 0) == at(0, 1, 1)).toBeTruthy();
            expect(clock.position(48000, 0) == at(1, 1, 1)).toBeTruthy();
        });

        it("subtracts the device latency", {
            PlaybackClock clock(hz(48000));
            expect(clock.anchor(at(10, 1, 1), 1000).has_value()).toBeTruthy();
            // 1000 + 48000 consumed, 2400 still buffered: 10 s + 45600 samples.
            expect(clock.position(49000, 2400) == at(10 * 48000 + 45600, 1, 48000)).toBeTruthy();
        });

        it("never reports a position before the anchor", {
            PlaybackClock clock(hz(44100));
            expect(clock.anchor(at(5, 1, 1), 500).has_value()).toBeTruthy();
            expect(clock.position(600, 1024) == at(5, 1, 1)).toBeTruthy();
        });

        it("rounds a video anchor to the nearest sample", {
            PlaybackClock clock(hz(44100));
            // Frame 1 at 29.97 fps is 1001/30000 s = 1471.47 samples at 44.1 kHz.
            expect(clock.anchor(at(1, 1001, 30000), 0).has_value()).toBeTruthy();
            expect(clock.anchor_sample()).toBe(1471LL);
        });

        it("re-anchors after a seek relative to the new consumed count", {
            PlaybackClock clock(hz(96000));
            expect(clock.anchor(at(0, 1, 1), 0).has_value()).toBeTruthy();
            expect(clock.anchor(at(3600, 1, 1), 192000).has_value()).toBeTruthy();
            expect(clock.position(288000, 0) == at(3601, 1, 1)).toBeTruthy();
        });

        it("reports overflow for an anchor beyond the sample range", {
            PlaybackClock clock(hz(96000));
            const auto huge = at(INT64_MAX / 2, 1, 1);
            expect(clock.anchor(huge, 0).has_value()).toBeFalsy();
        });
    });
}
