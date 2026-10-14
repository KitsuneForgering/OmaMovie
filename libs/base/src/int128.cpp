#include "int128.hpp"

namespace oma::detail {

std::optional<Int128> mul_checked(Int128 a, Int128 b) noexcept {
    Int128 result = 0;
    if (__builtin_mul_overflow(a, b, &result)) {
        return std::nullopt;
    }
    return result;
}

} // namespace oma::detail
