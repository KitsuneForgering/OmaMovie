#pragma once

// An exact fraction for the property oracles, independent of oma::Rational so a bug there
// cannot hide one in the checks. Generated values are small; 128 bits leave a wide margin.

#include "oma/base/time.hpp"

namespace property {

struct Fraction {
    __int128 n = 0;
    __int128 d = 1;
};

constexpr Fraction reduce(__int128 n, __int128 d) {
    if (d < 0) {
        n = -n;
        d = -d;
    }
    __int128 a = n < 0 ? -n : n;
    __int128 b = d;
    while (b != 0) {
        const __int128 r = a % b;
        a = b;
        b = r;
    }
    return a == 0 ? Fraction{0, 1} : Fraction{n / a, d / a};
}
constexpr Fraction operator+(Fraction x, Fraction y) {
    return reduce((x.n * y.d) + (y.n * x.d), x.d * y.d);
}
constexpr Fraction operator*(Fraction x, Fraction y) {
    return reduce(x.n * y.n, x.d * y.d);
}
constexpr bool operator<(Fraction x, Fraction y) {
    return x.n * y.d < y.n * x.d;
}
constexpr bool operator<=(Fraction x, Fraction y) {
    return !(y < x);
}

constexpr Fraction of(oma::Rational r) {
    return reduce(r.num(), r.den());
}
// Seconds of a time, exactly.
constexpr Fraction seconds(const oma::RationalTime& t) {
    return reduce(static_cast<__int128>(t.value()) * t.timebase().num(), t.timebase().den());
}

} // namespace property
