#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rescale.hpp"

#include <compare>
#include <cstdint>
#include <limits>
#include <numeric>

namespace oma {

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
        return {num / g, den / g};
    }

    [[nodiscard]] constexpr std::int64_t num() const noexcept { return num_; }
    [[nodiscard]] constexpr std::int64_t den() const noexcept { return den_; }
    [[nodiscard]] constexpr bool is_positive() const noexcept { return num_ > 0; }

    [[nodiscard]] Result<Rational> inverse() const;

    // Approximation for display and logging only; never use it for timing decisions.
    [[nodiscard]] double to_double_approx() const noexcept;

    friend constexpr bool operator==(Rational, Rational) noexcept = default;
    friend std::strong_ordering operator<=>(Rational a, Rational b) noexcept;

private:
    constexpr Rational(std::int64_t num, std::int64_t den) noexcept : num_(num), den_(den) {}

    std::int64_t num_ = 0;
    std::int64_t den_ = 1;
};

} // namespace oma
