#pragma once

// A layer's color adjustments and filter reduced to what both compositors evaluate per pixel,
// in linear BT.709 light:
//   c = max(c * gains, 0)                         white balance and exposure
//   c = pivot * (c / pivot) ^ power               contrast around 18% grey (when power != 1)
//   c = matrix * c + offset                       saturation and the filter's look
//   c = c * (1 - vignette * smoothstep(...))      the vignette filter
// Keep shaders/composite.comp and the CPU reference (src/cpu_compositor.cpp) in step with this.

#include "oma/compositor/render_graph.hpp"

#include <array>
#include <cstdint>

namespace oma::compositor {

inline constexpr double kContrastPivot = 0.18;
// Distances (1 at the corners of the cropped source) where the vignette starts and is full.
inline constexpr double kVignetteStart = 0.45;
inline constexpr double kVignetteEnd = 1.0;
inline constexpr double kVignetteDepth = 0.85; // darkening at the corners, full amount

struct Look {
    std::array<double, 3> gains{1.0, 1.0, 1.0};
    double power = 1.0;
    std::array<std::array<double, 4>, 3> matrix{
        {{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}}}; // rows; [3] is offset
    double vignette = 0.0; // darkening at the corners

    [[nodiscard]] bool identity() const noexcept;
};

[[nodiscard]] Look make_look(const ColorAdjust& color, const Filter& filter);

// Blur and sharpen (Layer::sharpness). The cropped source is linearized once and reduced by
// `factor` (a factor x factor box average per texel), then a separable gaussian runs across and
// down on the reduced image, its taps clamped to it; the composite reads it back bilinearly.
// The gaussian's sigma shrinks with the factor, so the cost no longer grows with the blur radius
// (Docs/Research/gpu-effects.md). Blur shows the blurred image; sharpen adds
// amount * (original - blurred).
// The factor is at least the source pixels per output pixel (a 2160p source shown at 1080p
// blurs at 1080p), else the reduced sigma stays below 2 * kReducedSigma, so 3 sigma fits the
// radius.
inline constexpr double kReducedSigma = 3.0;
inline constexpr int kMaxBlurRadius = 18;

enum class DetailMode : std::uint8_t {
    None = 0,
    Blur = 1,
    Sharpen = 2,
};

struct Detail {
    DetailMode mode = DetailMode::None;
    double amount = 0.0;                              // sharpen strength
    int radius = 0;                                   // taps on each side of the center
    int factor = 1;                                   // source texels per reduced texel, per axis
    std::array<double, kMaxBlurRadius + 1> weights{}; // center first, normalized over 2r + 1 taps
};

[[nodiscard]] Detail make_detail(double sharpness, std::uint32_t source_height,
                                 double source_per_output);

} // namespace oma::compositor
