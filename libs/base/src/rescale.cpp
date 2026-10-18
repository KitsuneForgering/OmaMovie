#include "oma/base/rational.hpp"

#include "int128.hpp"

#include <format>

namespace oma {

Result<std::int64_t> rescale(std::int64_t value, Rational from, Rational to, Rounding rounding) {
    using detail::Int128;
    if (!from.is_positive() || !to.is_positive()) {
        return make_error(
            ErrorCode::InvalidArgument, Category::Base, "rescale units must be positive",
            std::format("{}/{} -> {}/{}", from.num(), from.den(), to.num(), to.den()));
    }

    // value * (from.num * to.den) / (from.den * to.num); each factor fits in 126 bits.
    Int128 num = static_cast<Int128>(from.num()) * to.den();
    Int128 den = static_cast<Int128>(from.den()) * to.num();
    const Int128 g = detail::gcd128(num, den);
    num /= g;
    den /= g;

    const auto scaled = detail::mul_checked(value, num);
    if (!scaled) {
        return make_error(
            ErrorCode::Overflow, Category::Base, "rescale overflowed 128 bits",
            std::format("{} * {}/{} -> {}/{}", value, from.num(), from.den(), to.num(), to.den()));
    }
    const Int128 result = detail::div_round(*scaled, den, rounding);
    if (!detail::fits_int64(result)) {
        return make_error(
            ErrorCode::Overflow, Category::Base, "rescaled value does not fit in int64",
            std::format("{} * {}/{} -> {}/{}", value, from.num(), from.den(), to.num(), to.den()));
    }
    return static_cast<std::int64_t>(result);
}

} // namespace oma
