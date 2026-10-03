#include "oma/compositor/geometry.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace oma::compositor {

Affine Affine::translate(double x, double y) noexcept {
    return {.a = 1, .b = 0, .c = 0, .d = 1, .tx = x, .ty = y};
}

Affine Affine::scale(double sx, double sy) noexcept {
    return {.a = sx, .b = 0, .c = 0, .d = sy, .tx = 0, .ty = 0};
}

Affine Affine::rotate(double degrees) noexcept {
    // Exact quarter turns keep pixel grids aligned (no 1e-17 residue from sin/cos).
    const double turns = degrees / 90.0;
    if (turns == std::round(turns)) {
        const long q = ((static_cast<long>(std::round(turns)) % 4) + 4) % 4;
        constexpr std::array<double, 4> kCos{1, 0, -1, 0};
        constexpr std::array<double, 4> kSin{0, 1, 0, -1};
        const double cs = kCos[static_cast<std::size_t>(q)];
        const double sn = kSin[static_cast<std::size_t>(q)];
        return {.a = cs, .b = -sn, .c = sn, .d = cs, .tx = 0, .ty = 0};
    }
    const double r = degrees * std::numbers::pi / 180.0;
    const double cs = std::cos(r);
    const double sn = std::sin(r);
    // With y pointing down, this matrix turns clockwise on screen.
    return {.a = cs, .b = -sn, .c = sn, .d = cs, .tx = 0, .ty = 0};
}

Affine Affine::then(const Affine& n) const noexcept {
    return {.a = (n.a * a) + (n.b * c),
            .b = (n.a * b) + (n.b * d),
            .c = (n.c * a) + (n.d * c),
            .d = (n.c * b) + (n.d * d),
            .tx = (n.a * tx) + (n.b * ty) + n.tx,
            .ty = (n.c * tx) + (n.d * ty) + n.ty};
}

std::array<double, 2> Affine::apply(double x, double y) const noexcept {
    return {(a * x) + (b * y) + tx, (c * x) + (d * y) + ty};
}

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
