#include "oma/base/time.hpp"

#include "int128.hpp"

#include <format>

namespace oma {

namespace {

std::string format_time(const RationalTime& t) {
    return std::format("{} @ {}/{}", t.value(), t.timebase().num(), t.timebase().den());
}

} // namespace

// ---------------------------------------------------------------- RationalTime

Result<RationalTime> RationalTime::make(std::int64_t value, Rational timebase) {
    if (!timebase.is_positive()) {
        return make_error(ErrorCode::InvalidArgument, Category::Base, "timebase must be positive",
                          std::format("{}/{}", timebase.num(), timebase.den()));
    }
    return RationalTime(value, timebase);
}

Result<RationalTime> RationalTime::rescaled(Rational timebase, Rounding rounding) const {
    auto value = rescale(value_, timebase_, timebase, rounding);
    if (!value) {
        return std::unexpected(std::move(value.error()));
    }
    return make(*value, timebase);
}

Result<RationalTime> RationalTime::plus(const RationalTime& other) const {
    if (timebase_ != other.timebase_) {
        return make_error(ErrorCode::InvalidArgument, Category::Base,
                          "adding times with different timebases; rescale explicitly first",
                          format_time(*this) + " + " + format_time(other));
    }
    std::int64_t sum = 0;
    if (__builtin_add_overflow(value_, other.value_, &sum)) {
        return make_error(ErrorCode::Overflow, Category::Base, "time addition overflowed",
                          format_time(*this) + " + " + format_time(other));
    }
    return RationalTime(sum, timebase_);
}

Result<RationalTime> RationalTime::minus(const RationalTime& other) const {
    if (timebase_ != other.timebase_) {
        return make_error(ErrorCode::InvalidArgument, Category::Base,
                          "subtracting times with different timebases; rescale explicitly first",
                          format_time(*this) + " - " + format_time(other));
    }
    std::int64_t diff = 0;
    if (__builtin_sub_overflow(value_, other.value_, &diff)) {
        return make_error(ErrorCode::Overflow, Category::Base, "time subtraction overflowed",
                          format_time(*this) + " - " + format_time(other));
    }
    return RationalTime(diff, timebase_);
}

double RationalTime::seconds_approx() const noexcept {
    return static_cast<double>(value_) * timebase_.to_double_approx();
}

std::strong_ordering operator<=>(const RationalTime& a, const RationalTime& b) noexcept {
    using detail::i128;
    // value * num fits in 126 bits; compare_fractions never forms larger products.
    const int c = detail::compare_fractions(
        static_cast<i128>(a.value_) * a.timebase_.num(), a.timebase_.den(),
        static_cast<i128>(b.value_) * b.timebase_.num(), b.timebase_.den());
    return c < 0 ? std::strong_ordering::less
                 : (c > 0 ? std::strong_ordering::greater : std::strong_ordering::equal);
}

// ---------------------------------------------------------------- TimeRange

Result<TimeRange> TimeRange::make(RationalTime start, RationalTime duration) {
    if (start.timebase() != duration.timebase()) {
        return make_error(ErrorCode::InvalidArgument, Category::Base,
                          "range start and duration must share a timebase",
                          format_time(start) + ", " + format_time(duration));
    }
    if (duration.value() < 0) {
        return make_error(ErrorCode::InvalidArgument, Category::Base, "negative range duration",
                          format_time(duration));
    }
    auto end = start.plus(duration);
    if (!end) {
        return std::unexpected(std::move(end.error()));
    }
    return TimeRange(start, duration, *end);
}

bool TimeRange::contains(const RationalTime& t) const noexcept {
    return start_ <= t && t < end_;
}

bool TimeRange::overlaps(const TimeRange& other) const noexcept {
    return start_ < other.end_ && other.start_ < end_;
}

// ---------------------------------------------------------------- FrameRate

Result<FrameRate> FrameRate::make(Rational fps) {
    if (!fps.is_positive()) {
        return make_error(ErrorCode::InvalidArgument, Category::Base, "frame rate must be positive",
                          std::format("{}/{}", fps.num(), fps.den()));
    }
    return FrameRate(fps);
}

Rational FrameRate::frame_duration() const {
    // fps is normalized and positive, so its inverse is always valid.
    return *fps_.inverse();
}

RationalTime FrameRate::frame_to_time(std::int64_t frame) const {
    return *RationalTime::make(frame, frame_duration());
}

Result<std::int64_t> FrameRate::time_to_frame(const RationalTime& t, Rounding rounding) const {
    return rescale(t.value(), t.timebase(), frame_duration(), rounding);
}

// ---------------------------------------------------------------- SampleRate

Result<SampleRate> SampleRate::make(std::int32_t hz) {
    if (hz <= 0) {
        return make_error(ErrorCode::InvalidArgument, Category::Base,
                          "sample rate must be positive", std::format("{} Hz", hz));
    }
    return SampleRate(hz);
}

Rational SampleRate::timebase() const {
    return *Rational::make(1, hz_);
}

RationalTime SampleRate::sample_to_time(std::int64_t sample) const {
    return *RationalTime::make(sample, timebase());
}

Result<std::int64_t> SampleRate::time_to_sample(const RationalTime& t, Rounding rounding) const {
    return rescale(t.value(), t.timebase(), timebase(), rounding);
}

} // namespace oma
