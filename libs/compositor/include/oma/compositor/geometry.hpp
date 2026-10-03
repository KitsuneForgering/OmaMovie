#pragma once

#include <array>
#include <optional>

// 2D affine transforms in pixel space. Coordinates are continuous: pixel (i, j) covers
// [i, i + 1) x [j, j + 1), so its center is (i + 0.5, j + 0.5). The y axis points down.

namespace oma::compositor {

struct Affine {
    // x' = a * x + b * y + tx
    // y' = c * x + d * y + ty
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double tx = 0.0;
    double ty = 0.0;

    [[nodiscard]] static Affine translate(double x, double y) noexcept;
    [[nodiscard]] static Affine scale(double sx, double sy) noexcept;
    // Clockwise on screen (the y axis points down).
    [[nodiscard]] static Affine rotate(double degrees) noexcept;

    // This transform followed by `next`.
    [[nodiscard]] Affine then(const Affine& next) const noexcept;
    [[nodiscard]] std::array<double, 2> apply(double x, double y) const noexcept;
    // std::nullopt when the transform collapses the plane (zero scale).
    [[nodiscard]] std::optional<Affine> inverse() const noexcept;
};

} // namespace oma::compositor
