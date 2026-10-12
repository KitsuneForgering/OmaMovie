#include "oma/compositor/geometry.hpp"

#include <cmath>

namespace oma::compositor {

std::optional<Affine> Affine::inverse() const noexcept {
    const double det = (a * d) - (b * c);
    if (std::abs(det) < 1e-12) {
        return std::nullopt;
    }
    const double ia = d / det;
    const double ib = -b / det;
    const double ic = -c / det;
    const double id = a / det;
    return Affine{.a = ia,
                  .b = ib,
                  .c = ic,
                  .d = id,
                  .tx = -((ia * tx) + (ib * ty)),
                  .ty = -((ic * tx) + (id * ty))};
}

} // namespace oma::compositor
