#pragma once

// 128-bit helpers for exact time arithmetic. Internal to libs/base.

#include "oma/base/rational.hpp"

#include <cstdint>
#include <limits>
#include <optional>

namespace oma::detail {

__extension__ using i128 = __int128;

[[nodiscard]] constexpr i128 abs128(i128 v) noexcept {
    // Callers only pass values derived from int64 products, never the i128 minimum.
    return v < 0 ? -v : v;
}

[[nodiscard]] constexpr i128 gcd128(i128 a, i128 b) noexcept {
    a = abs128(a);
    b = abs128(b);
    while (b != 0) {
        const i128 t = a % b;
        a = b;
        b = t;
    }
    return a;
}

[[nodiscard]] inline std::optional<i128> mul_checked(i128 a, i128 b) noexcept {
    i128 r = 0;
    if (__builtin_mul_overflow(a, b, &r)) {
        return std::nullopt;
    }
    return r;
}

// Floor division for a divisor d > 0.
[[nodiscard]] constexpr i128 floor_div(i128 n, i128 d) noexcept {
    i128 q = n / d;
    if (n % d != 0 && n < 0) {
        --q;
    }
    return q;
}

// n / d rounded as requested, for a divisor d > 0.
[[nodiscard]] constexpr i128 div_round(i128 n, i128 d, Rounding rounding) noexcept {
    const i128 q = n / d;   // truncates toward zero
    const i128 rem = n % d; // same sign as n
    if (rem == 0) {
        return q;
    }
    switch (rounding) {
    case Rounding::Floor:
        return n < 0 ? q - 1 : q;
    case Rounding::Ceil:
        return n > 0 ? q + 1 : q;
    case Rounding::Nearest: {
        // |rem| >= d - |rem| is the overflow-free form of 2|rem| >= d (ties away from zero).
        const i128 a = abs128(rem);
        if (a >= d - a) {
            return n < 0 ? q - 1 : q + 1;
        }
        return q;
    }
    }
    return q;
}

// Exact comparison of p/q and r/s (q > 0, s > 0) without forming cross products,
// which could exceed 128 bits. Returns -1, 0 or 1. Terminates like Euclid's algorithm.
[[nodiscard]] constexpr int compare_fractions(i128 p, i128 q, i128 r, i128 s) noexcept {
    int sign = 1;
    for (;;) {
        const i128 a = floor_div(p, q);
        const i128 b = floor_div(r, s);
        if (a != b) {
            return a < b ? -sign : sign;
        }
        const i128 pr = p - a * q; // in [0, q)
        const i128 rr = r - b * s; // in [0, s)
        if (pr == 0 || rr == 0) {
            if (pr == rr) {
                return 0;
            }
            return pr == 0 ? -sign : sign;
        }
        // Both remainders are in (0, 1): pr/q < rr/s  <=>  q/pr > s/rr.
        // Compare the reciprocals and flip the sign of the answer.
        const i128 next_p = q;
        const i128 next_q = pr;
        const i128 next_r = s;
        const i128 next_s = rr;
        p = next_p;
        q = next_q;
        r = next_r;
        s = next_s;
        sign = -sign;
    }
}

[[nodiscard]] constexpr bool fits_int64(i128 v) noexcept {
    return v >= std::numeric_limits<std::int64_t>::min() &&
           v <= std::numeric_limits<std::int64_t>::max();
}

} // namespace oma::detail
