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
