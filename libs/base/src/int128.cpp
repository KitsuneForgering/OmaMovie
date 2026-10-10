#include "int128.hpp"

namespace oma::detail {

std::optional<i128> mul_checked(i128 a, i128 b) noexcept {
    i128 result = 0;
    if (__builtin_mul_overflow(a, b, &result)) {
        return std::nullopt;
    }
    return result;
}

} // namespace oma::detail
