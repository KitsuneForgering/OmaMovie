#include "mutation.hpp"

#include <algorithm>
#include <format>
#include <limits>

namespace oma::timeline::detail {

namespace {

__extension__ using Int128 = __int128;

Int128 gcd128(Int128 a, Int128 b) {
    a = a < 0 ? -a : a;
    b = b < 0 ? -b : b;
    while (b != 0) {
        const Int128 r = a % b;
        a = b;
        b = r;
    }
    return a;
}

bool fits(Int128 v) {
    return v >= std::numeric_limits<std::int64_t>::min() + 1 &&
           v <= std::numeric_limits<std::int64_t>::max();
}

Result<Rational> reduced(Int128 num, Int128 den) {
    const Int128 g = gcd128(num, den);
    if (g != 0) {
        num /= g;
        den /= g;
    }
    if (!fits(num) || !fits(den)) {
        return error(ErrorCode::Overflow, "rational term exceeds 64 bits");
    }
    return Rational::make(static_cast<std::int64_t>(num), static_cast<std::int64_t>(den));
}

} // namespace

Track* Mutation::track(TrackId id) noexcept {
    const auto it = std::ranges::find(t_.tracks_, id, &Track::id);
    return it == t_.tracks_.end() ? nullptr : &*it;
}

Result<Rational> multiply(Rational a, Rational b) {
    return reduced(static_cast<Int128>(a.num()) * b.num(), static_cast<Int128>(a.den()) * b.den());
}

Result<Rational> add(Rational a, Rational b) {
    return reduced((static_cast<Int128>(a.num()) * b.den()) +
                       (static_cast<Int128>(b.num()) * a.den()),
                   static_cast<Int128>(a.den()) * b.den());
}

namespace {

// Checked 128-bit arithmetic: false on overflow.
bool mul(Int128 a, Int128 b, Int128& out) {
    return !__builtin_mul_overflow(a, b, &out);
}
bool sum(Int128 a, Int128 b, Int128& out) {
    return !__builtin_add_overflow(a, b, &out);
}

// (to - from) over the denominator df*dt: delta = to.num*df - from.num*dt.
bool ramp_delta(const TimeSegment& s, Int128& delta) {
    Int128 x = 0;
    Int128 y = 0;
    return mul(s.to.num(), s.from.den(), x) && mul(s.from.num(), s.to.den(), y) &&
           sum(x, -y, delta);
}

std::unexpected<Error> ramp_overflow() {
    return error(ErrorCode::Overflow, "speed ramp too long to evaluate exactly");
}

} // namespace

Result<Rational> displacement(const TimeSegment& s, std::int64_t t) {
    switch (s.kind) {
    case TimeSegment::Kind::Linear:
        return reduced(static_cast<Int128>(s.from.num()) * t, s.from.den());
    case TimeSegment::Kind::Freeze:
        return Rational::make(0, 1);
    case TimeSegment::Kind::Ramp: {
        // from*t + (to - from)*t^2/(2L) over the common denominator 2L*df*dt.
        Int128 delta = 0;
        Int128 two_l = 0;
        Int128 a = 0;
        Int128 b = 0;
        Int128 num = 0;
        Int128 den = 0;
        Int128 tmp = 0;
        if (!ramp_delta(s, delta) || !mul(2, s.length, two_l) || !mul(two_l, s.from.num(), tmp) ||
            !mul(tmp, s.to.den(), tmp) || !mul(tmp, t, a) || !mul(delta, t, b) || !mul(b, t, b) ||
            !sum(a, b, num) || !mul(two_l, s.from.den(), den) || !mul(den, s.to.den(), den)) {
            return ramp_overflow();
        }
        return reduced(num, den);
    }
    }
    return error(ErrorCode::InvalidData, "unknown time segment");
}

Result<Rational> speed_at(const TimeSegment& s, std::int64_t t) {
    switch (s.kind) {
    case TimeSegment::Kind::Linear:
        return s.from;
    case TimeSegment::Kind::Freeze:
        return Rational::make(0, 1);
    case TimeSegment::Kind::Ramp: {
        // from + (to - from)*t/L over the denominator df*dt*L.
        Int128 delta = 0;
        Int128 a = 0;
        Int128 b = 0;
        Int128 num = 0;
        Int128 den = 0;
        if (!ramp_delta(s, delta) || !mul(s.from.num(), s.to.den(), a) || !mul(a, s.length, a) ||
            !mul(delta, t, b) || !sum(a, b, num) || !mul(s.from.den(), s.to.den(), den) ||
            !mul(den, s.length, den)) {
            return ramp_overflow();
        }
        return reduced(num, den);
    }
    }
    return error(ErrorCode::InvalidData, "unknown time segment");
}

Result<Rational> common_unit(Rational a, Rational b) {
    // gcd(p/q, r/s) = gcd(p*s, r*q) / (q*s) for positive fractions.
    const Int128 num =
        gcd128(static_cast<Int128>(a.num()) * b.den(), static_cast<Int128>(b.num()) * a.den());
    return reduced(num, static_cast<Int128>(a.den()) * b.den());
}

Result<RationalTime> add_exact(const RationalTime& a, const RationalTime& b) {
    if (a.timebase() == b.timebase()) {
        return a.plus(b);
    }
    auto unit = common_unit(a.timebase(), b.timebase());
    if (!unit) {
        return std::unexpected(unit.error());
    }
    // Both timebases are integer multiples of the unit, so these conversions are exact.
    auto va = rescale(a.value(), a.timebase(), *unit, Rounding::Floor);
    auto vb = rescale(b.value(), b.timebase(), *unit, Rounding::Floor);
    if (!va || !vb) {
        return std::unexpected(!va ? va.error() : vb.error());
    }
    auto sum = add_ticks(*va, *vb);
    if (!sum) {
        return std::unexpected(sum.error());
    }
    return RationalTime::make(*sum, *unit);
}

Result<std::int64_t> add_ticks(std::int64_t a, std::int64_t b) {
    std::int64_t out = 0;
    if (__builtin_add_overflow(a, b, &out)) {
        return error(ErrorCode::Overflow, "timeline position overflow",
                     std::format("{} + {}", a, b));
    }
    return out;
}

Result<RationalTime> source_end(const Clip& clip, Rational sequence_timebase) {
    auto length = clip.time_map.media_offset(clip.duration.value(), sequence_timebase);
    if (!length) {
        return std::unexpected(length.error());
    }
    return add_exact(clip.source_in, *length);
}

Result<std::int64_t> attach_tick(const Clip& p, const RationalTime& source,
                                 Rational sequence_timebase) {
    auto unit = multiply(sequence_timebase, p.time_map.speed());
    auto back = RationalTime::make(-p.source_in.value(), p.source_in.timebase());
    if (!unit || !back) {
        return std::unexpected(!unit ? unit.error() : back.error());
    }
    auto diff = add_exact(source, *back);
    if (!diff) {
        return std::unexpected(diff.error());
    }
    auto local = rescale(diff->value(), diff->timebase(), *unit, Rounding::Ceil);
    if (!local) {
        return std::unexpected(local.error());
    }
    return add_ticks(p.start_ticks(), *local);
}

Result<RationalTime> source_at(const Clip& p, std::int64_t tick, Rational sequence_timebase) {
    auto offset = p.time_map.media_offset(tick - p.start_ticks(), sequence_timebase);
    if (!offset) {
        return std::unexpected(offset.error());
    }
    return add_exact(p.source_in, *offset);
}

bool holds(const Clip& p, const RationalTime& source, Rational sequence_timebase) {
    auto end = source_end(p, sequence_timebase);
    return end && !(source < p.source_in) && source < *end;
}

std::unexpected<Error> error(ErrorCode code, std::string message, std::string context) {
    return make_error(code, Category::Timeline, std::move(message), std::move(context));
}

std::string clip_context(ClipId id) {
    return std::format("clip {}", id.value());
}

std::string track_context(TrackId id) {
    return std::format("track {}", id.value());
}

} // namespace oma::timeline::detail
