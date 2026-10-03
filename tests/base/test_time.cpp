#include "oma/base/time.hpp"

#include <cstdint>
#include <limits>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::FrameRate;
using oma::Rational;
using oma::RationalTime;
using oma::Rounding;
using oma::SampleRate;
using oma::TimeRange;
namespace fr = oma::frame_rates;

namespace {

constexpr Rational kTb90k = Rational::literal(1, 90000);
constexpr Rational kTb30k = Rational::literal(1, 30000);
constexpr Rational kTb48k = Rational::literal(1, 48000);
constexpr Rational kSecond = Rational::literal(1, 1);

RationalTime t(std::int64_t value, Rational tb) {
    return *RationalTime::make(value, tb);
}

int code_of(const oma::Error& e) {
    return static_cast<int>(e.code());
}

} // namespace

void run_time_tests() {
    describe("RationalTime", {
        it("compares instants exactly across timebases", {
            expect(t(3003, kTb90k) == t(1001, kTb30k)).toBeTruthy();
            expect(t(48000, kTb48k) == t(90000, kTb90k)).toBeTruthy();
            expect(t(1, kTb48k) < t(2, kTb90k)).toBeTruthy();
            expect(t(-1, kTb48k) < t(0, kTb90k)).toBeTruthy();
        });

        it("compares extreme values without overflowing", {
            const std::int64_t max = std::numeric_limits<std::int64_t>::max();
            expect(t(max, kSecond) > t(max, Rational::literal(1, 2))).toBeTruthy();
            expect(t(max, kTb90k) < t(max, kTb48k)).toBeTruthy();
            expect(t(max - 1, Rational::literal(1, max)) < t(1, kSecond)).toBeTruthy();
        });

        it("rejects a non-positive timebase", {
            expect(RationalTime::make(1, Rational::literal(0, 1)).has_value()).toBeFalsy();
            expect(RationalTime::make(1, Rational::literal(-1, 25)).has_value()).toBeFalsy();
        });

        it("rescales with explicit rounding", {
            auto r = t(1, fr::k23_976.frame_duration()).rescaled(kTb90k, Rounding::Nearest);
            expect(r->value()).toEqual(3754);
            expect(r->timebase() == kTb90k).toBeTruthy();
        });

        it("adds only within one timebase", {
            expect(t(10, kTb90k).plus(t(5, kTb90k))->value()).toEqual(15);
            auto bad = t(10, kTb90k).plus(t(5, kTb48k));
            expect(code_of(bad.error())).toEqual(static_cast<int>(ErrorCode::InvalidArgument));
        });

        it("detects overflow in addition", {
            const std::int64_t max = std::numeric_limits<std::int64_t>::max();
            auto r = t(max, kTb90k).plus(t(1, kTb90k));
            expect(code_of(r.error())).toEqual(static_cast<int>(ErrorCode::Overflow));
        });
    });

    describe("TimeRange", {
        it("is half-open", {
            auto range = TimeRange::make(t(100, kTb90k), t(50, kTb90k));
            expect(range->contains(t(100, kTb90k))).toBeTruthy();
            expect(range->contains(t(149, kTb90k))).toBeTruthy();
            expect(range->contains(t(150, kTb90k))).toBeFalsy();
            expect(range->contains(t(99, kTb90k))).toBeFalsy();
            expect(range->end().value()).toEqual(150);
        });

        it("checks membership across timebases", {
            auto range = TimeRange::make(t(0, kSecond), t(1, kSecond));
            expect(range->contains(t(89999, kTb90k))).toBeTruthy();
            expect(range->contains(t(90000, kTb90k))).toBeFalsy();
        });

        it("detects overlap", {
            auto a = TimeRange::make(t(0, kTb90k), t(10, kTb90k));
            auto b = TimeRange::make(t(10, kTb90k), t(10, kTb90k));
            auto c = TimeRange::make(t(9, kTb90k), t(10, kTb90k));
            expect(a->overlaps(*b)).toBeFalsy();
            expect(a->overlaps(*c)).toBeTruthy();
        });

        it("rejects negative durations and mixed timebases", {
            expect(TimeRange::make(t(0, kTb90k), t(-1, kTb90k)).has_value()).toBeFalsy();
            expect(TimeRange::make(t(0, kTb90k), t(1, kTb48k)).has_value()).toBeFalsy();
        });
    });

    describe("FrameRate", {
        it("represents NTSC rates as exact fractions", {
            expect(fr::k29_97.fps() == Rational::literal(30000, 1001)).toBeTruthy();
            expect(fr::k59_94.frame_duration() == Rational::literal(1001, 60000)).toBeTruthy();
        });

        it("maps frame 300 at 29.97 fps to 10.01 s", {
            const RationalTime at = fr::k29_97.frame_to_time(300);
            expect(at == t(1001, Rational::literal(1, 100))).toBeTruthy();
        });

        it("maps a time back to a frame with explicit rounding", {
            expect(*fr::k29_97.time_to_frame(t(10, kSecond), Rounding::Floor)).toEqual(299);
            expect(*fr::k29_97.time_to_frame(t(10, kSecond), Rounding::Nearest)).toEqual(300);
            expect(*fr::k25.time_to_frame(t(10, kSecond), Rounding::Floor)).toEqual(250);
        });

        it("rejects non-positive rates",
           { expect(FrameRate::make(Rational::literal(0, 1)).has_value()).toBeFalsy(); });
    });

    describe("SampleRate", {
        it("addresses audio in samples", {
            auto sr = SampleRate::make(48000);
            expect(*sr->time_to_sample(t(1, kSecond), Rounding::Floor)).toEqual(48000);
            expect(sr->sample_to_time(24000) == t(1, Rational::literal(1, 2))).toBeTruthy();
        });

        it("converts 44.1 kHz positions to 96 kHz", {
            auto sr96 = SampleRate::make(96000);
            const RationalTime pos = SampleRate::make(44100)->sample_to_time(44100);
            expect(*sr96->time_to_sample(pos, Rounding::Floor)).toEqual(96000);
        });

        it("rejects non-positive rates", {
            expect(SampleRate::make(0).has_value()).toBeFalsy();
            expect(SampleRate::make(-48000).has_value()).toBeFalsy();
        });
    });
}
