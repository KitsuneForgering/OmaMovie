#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"

#include <compare>
#include <cstdint>

namespace oma {

// An instant: value * timebase seconds (CLAUDE.md §6). The timebase is always positive.
//
// Comparison is exact across timebases: 3003 @ 1/90000 == 1001 @ 1/30000.
// Arithmetic only combines times that share a timebase; convert explicitly first.
class RationalTime {
public:
    constexpr RationalTime() noexcept = default;

    [[nodiscard]] static Result<RationalTime> make(std::int64_t value, Rational timebase);

    [[nodiscard]] constexpr std::int64_t value() const noexcept { return value_; }
    [[nodiscard]] constexpr Rational timebase() const noexcept { return timebase_; }

    [[nodiscard]] Result<RationalTime> rescaled(Rational timebase, Rounding rounding) const;
    [[nodiscard]] Result<RationalTime> plus(const RationalTime& other) const;
    [[nodiscard]] Result<RationalTime> minus(const RationalTime& other) const;

    // Approximation for display only (timecode labels, logs).
    [[nodiscard]] double seconds_approx() const noexcept;

    friend std::strong_ordering operator<=>(const RationalTime& a, const RationalTime& b) noexcept;
    friend bool operator==(const RationalTime& a, const RationalTime& b) noexcept {
        return (a <=> b) == 0;
    }

private:
    constexpr RationalTime(std::int64_t value, Rational timebase) noexcept
        : value_(value), timebase_(timebase) {}

    std::int64_t value_ = 0;
    Rational timebase_ = Rational::literal(1, 1);
};

// Half-open interval [start, start + duration). Start and duration share one timebase,
// so the end is exact.
class TimeRange {
public:
    [[nodiscard]] static Result<TimeRange> make(RationalTime start, RationalTime duration);

    [[nodiscard]] const RationalTime& start() const noexcept { return start_; }
    [[nodiscard]] const RationalTime& duration() const noexcept { return duration_; }
    [[nodiscard]] const RationalTime& end() const noexcept { return end_; }
    [[nodiscard]] bool empty() const noexcept { return duration_.value() == 0; }

    [[nodiscard]] bool contains(const RationalTime& t) const noexcept;
    [[nodiscard]] bool overlaps(const TimeRange& other) const noexcept;

private:
    TimeRange(RationalTime start, RationalTime duration, RationalTime end) noexcept
        : start_(start), duration_(duration), end_(end) {}

    RationalTime start_;
    RationalTime duration_;
    RationalTime end_;
};

// Frames per second as an exact fraction (29.97 is 30000/1001, never 29.97).
// Describes a nominal rate; media may be VFR, so frames are located by timestamp,
// never by index * duration (CLAUDE.md §6).
class FrameRate {
public:
    [[nodiscard]] static Result<FrameRate> make(Rational fps);
    [[nodiscard]] static consteval FrameRate literal(std::int64_t num, std::int64_t den) {
        const Rational fps = Rational::literal(num, den);
        if (!fps.is_positive()) {
            detail::invalid_rational_literal();
        }
        return FrameRate(fps);
    }

    [[nodiscard]] constexpr Rational fps() const noexcept { return fps_; }
    // Duration of one frame, also the natural timebase for frame counts.
    [[nodiscard]] Rational frame_duration() const;

    [[nodiscard]] RationalTime frame_to_time(std::int64_t frame) const;
    [[nodiscard]] Result<std::int64_t> time_to_frame(const RationalTime& t,
                                                     Rounding rounding) const;

    friend constexpr bool operator==(FrameRate, FrameRate) noexcept = default;

private:
    explicit constexpr FrameRate(Rational fps) noexcept : fps_(fps) {}

    Rational fps_;
};

namespace frame_rates {
inline constexpr FrameRate k23_976 = FrameRate::literal(24000, 1001);
inline constexpr FrameRate k24 = FrameRate::literal(24, 1);
inline constexpr FrameRate k25 = FrameRate::literal(25, 1);
inline constexpr FrameRate k29_97 = FrameRate::literal(30000, 1001);
inline constexpr FrameRate k30 = FrameRate::literal(30, 1);
inline constexpr FrameRate k50 = FrameRate::literal(50, 1);
inline constexpr FrameRate k59_94 = FrameRate::literal(60000, 1001);
inline constexpr FrameRate k60 = FrameRate::literal(60, 1);
} // namespace frame_rates

// Audio sample rate in Hz. Audio is addressed in samples, not video frames (CLAUDE.md §6).
class SampleRate {
public:
    [[nodiscard]] static Result<SampleRate> make(std::int32_t hz);

    [[nodiscard]] constexpr std::int32_t hz() const noexcept { return hz_; }
    [[nodiscard]] Rational timebase() const;

    [[nodiscard]] RationalTime sample_to_time(std::int64_t sample) const;
    [[nodiscard]] Result<std::int64_t> time_to_sample(const RationalTime& t,
                                                      Rounding rounding) const;

    friend constexpr bool operator==(SampleRate, SampleRate) noexcept = default;

private:
    explicit constexpr SampleRate(std::int32_t hz) noexcept : hz_(hz) {}

    std::int32_t hz_;
};

} // namespace oma
