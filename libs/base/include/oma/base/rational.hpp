#pragma once

#include "oma/base/error.hpp"

#include <compare>
#include <cstdint>
#include <limits>
#include <numeric>

namespace oma {

// How a conversion that cannot be represented exactly is rounded (CLAUDE.md §6).
enum class Rounding : std::uint8_t {
    Floor,   // toward negative infinity
    Ceil,    // toward positive infinity
    Nearest, // to the nearest integer; ties away from zero
};

namespace detail {
// Called only from consteval code: reaching it makes the constant expression ill-formed,
// turning an invalid literal into a compile error.
void invalid_rational_literal();
} // namespace detail

// Exact fraction num/den, always normalized: den > 0 and gcd(|num|, den) == 1.
// INT64_MIN is rejected in both terms so negation can never overflow.
class Rational {
public:
    constexpr Rational() noexcept = default;

    [[nodiscard]] static Result<Rational> make(std::int64_t num, std::int64_t den);

    // Compile-time constant, e.g. Rational::literal(1, 90000). Invalid input does not compile.
    [[nodiscard]] static consteval Rational literal(std::int64_t num, std::int64_t den) {
        constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
        if (den == 0 || num == kMin || den == kMin) {
            detail::invalid_rational_literal();
        }
        if (den < 0) {
            num = -num;
            den = -den;
        }
        const std::int64_t g = std::gcd(num, den);
        return Rational(num / g, den / g);
    }

    [[nodiscard]] constexpr std::int64_t num() const noexcept { return num_; }
    [[nodiscard]] constexpr std::int64_t den() const noexcept { return den_; }
    [[nodiscard]] constexpr bool is_positive() const noexcept { return num_ > 0; }

    [[nodiscard]] Result<Rational> inverse() const;

    // Approximation for display and logging only; never use it for timing decisions.
    [[nodiscard]] double to_double_approx() const noexcept;

    // Normalization makes member-wise equality exact equality.
    friend constexpr bool operator==(Rational, Rational) noexcept = default;
    friend std::strong_ordering operator<=>(Rational a, Rational b) noexcept;

private:
    constexpr Rational(std::int64_t num, std::int64_t den) noexcept : num_(num), den_(den) {}

    std::int64_t num_ = 0;
    std::int64_t den_ = 1;
};

// Converts `value` expressed in `from` units into `to` units:
//   value * from / to, rounded as requested.
// Both units must be positive. Intermediate math uses 128 bits; a result outside
// int64 returns ErrorCode::Overflow instead of truncating.
[[nodiscard]] Result<std::int64_t> rescale(std::int64_t value, Rational from, Rational to,
                                           Rounding rounding);

} // namespace oma
