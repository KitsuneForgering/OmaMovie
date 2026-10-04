#pragma once

// Color grading of a layer (ADR-0012): an ASC CDL, curves and a 3D LUT, applied in that order
// after the layer's look, in the grading space: BT.709 primaries re-encoded with the inverse of
// the layer's own transfer function and clamped to [0, 1], so they see the file's code values.

#include "oma/base/error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace oma::compositor {

// ASC CDL v1.2: per channel out = clamp(in * slope + offset)^power, then saturation around
// Rec.709 luma, clamped. Lift/gamma/gain map onto it exactly: slope = gain * (1 - lift),
// offset = gain * lift, power = gamma.
struct Cdl {
    std::array<double, 3> slope{1.0, 1.0, 1.0};  // in [0, 4]
    std::array<double, 3> offset{0.0, 0.0, 0.0}; // in [-1, 1]
    std::array<double, 3> power{1.0, 1.0, 1.0};  // in [0.1, 4]
    double saturation = 1.0;                     // in [0, 4]

    [[nodiscard]] bool identity() const noexcept;
    friend bool operator==(const Cdl&, const Cdl&) noexcept = default;
};

// A curve point in the grading space, both coordinates in [0, 1].
struct CurvePoint {
    double x = 0.0;
    double y = 0.0;
    friend bool operator==(const CurvePoint&, const CurvePoint&) noexcept = default;
};

inline constexpr std::size_t kMaxCurvePoints = 16;

// Monotone cubic curves (Fritsch–Carlson: no overshoot between points), flat beyond the first
// and last points. An empty curve is the identity; otherwise 2 to kMaxCurvePoints points with
// strictly increasing x. Each channel goes through master, then its own curve.
struct Curves {
    std::vector<CurvePoint> master;
    std::vector<CurvePoint> red;
    std::vector<CurvePoint> green;
    std::vector<CurvePoint> blue;

    [[nodiscard]] bool identity() const noexcept;
    friend bool operator==(const Curves&, const Curves&) noexcept = default;
};

// The tangent at each point of a valid curve (Fritsch–Carlson), what the shader interpolates with.
[[nodiscard]] std::vector<double> curve_tangents(std::span<const CurvePoint> points);
// The curve at `x`; the identity for an empty curve. For drawing and tests.
[[nodiscard]] double evaluate_curve(std::span<const CurvePoint> points, double x);

// A 3D lookup table: size^3 RGB entries, red fastest, mapping inputs in [domain_min,
// domain_max] (clamped there) to grading-space RGB. Interpolated tetrahedrally.
inline constexpr std::uint32_t kMaxLutSize = 65;
struct Lut3d {
    std::uint32_t size = 0; // 2 to kMaxLutSize
    std::array<float, 3> domain_min{0.0F, 0.0F, 0.0F};
    std::array<float, 3> domain_max{1.0F, 1.0F, 1.0F};
    std::vector<float> rgb; // size^3 * 3
};

// Parses a .cube file (Adobe Cube LUT 1.0 and Resolve's variant): LUT_3D_SIZE, DOMAIN_MIN/MAX or
// LUT_3D_INPUT_RANGE, TITLE and # comments. 1D LUTs are refused. Untrusted input: sizes, counts
// and numbers are checked and the text is capped at kMaxCubeBytes.
inline constexpr std::size_t kMaxCubeBytes = 16U << 20U;
[[nodiscard]] Result<Lut3d> parse_cube(std::string_view text);

struct Grade {
    Cdl cdl;
    Curves curves;
    std::shared_ptr<const Lut3d> lut; // shared and immutable: the compositor caches its upload
    double lut_amount = 1.0;          // in [0, 1]: mix between before and after the LUT

    [[nodiscard]] bool identity() const noexcept;
};

// Checks ranges, curve points and the LUT's shape.
[[nodiscard]] Result<void> validate(const Grade& grade);

} // namespace oma::compositor
