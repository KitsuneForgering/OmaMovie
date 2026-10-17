#pragma once

#include "oma/base/error.hpp"

#include <cstdint>

namespace oma {
class Rational;

// How a conversion that cannot be represented exactly is rounded (CLAUDE.md §6).
enum class Rounding : std::uint8_t {
    Floor,   // toward negative infinity
    Ceil,    // toward positive infinity
    Nearest, // to the nearest integer; ties away from zero
};

// Converts value from one positive unit to another with checked 128-bit arithmetic.
[[nodiscard]] Result<std::int64_t> rescale(std::int64_t value, Rational from, Rational to,
                                           Rounding rounding);
} // namespace oma
