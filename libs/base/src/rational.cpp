#include "oma/base/rational.hpp"

#include "int128.hpp"

#include <format>

namespace oma {

namespace detail {
void invalid_rational_literal() {}
} // namespace detail

namespace {
constexpr auto kInt64Min = std::numeric_limits<std::int64_t>::min();
} // namespace

Result<Rational> Rational::make(std::int64_t num, std::int64_t den) {
    if (den == 0) {
        return make_error(ErrorCode::InvalidArgument, Category::Base,
                          "rational with zero denominator", std::format("{}/{}", num, den));
    }
    if (num == kInt64Min || den == kInt64Min) {
        return make_error(ErrorCode::OutOfRange, Category::Base,
                          "INT64_MIN cannot be represented in a normalized rational",
                          std::format("{}/{}", num, den));
    }
    if (den < 0) {
        num = -num;
        den = -den;
    }
    const std::int64_t g = std::gcd(num, den);
    return Rational(num / g, den / g);
}

Result<Rational> Rational::inverse() const {
    return make(den_, num_);
}

double Rational::to_double_approx() const noexcept {
    return static_cast<double>(num_) / static_cast<double>(den_);
}

std::strong_ordering operator<=>(Rational a, Rational b) noexcept {
    const int c = detail::compare_fractions(a.num_, a.den_, b.num_, b.den_);
    return c < 0 ? std::strong_ordering::less
                 : (c > 0 ? std::strong_ordering::greater : std::strong_ordering::equal);
}

} // namespace oma
