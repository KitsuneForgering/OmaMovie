#pragma once

// Per-layer parameters shared by the CPU reference and the compute shader. The struct mirrors
// the std140 uniform block in shaders/composite.comp; change both together.

#include "look.hpp"

#include "oma/compositor/compositor.hpp"
#include "oma/media/video_frame.hpp"

#include <array>
#include <cstdint>

namespace oma::compositor {

enum class ChromaMode : std::uint8_t {
    Interleaved = 1, // plane 1 holds Cb and Cr (NV12, P010)
    Planar = 2,      // planes 1 and 2 hold Cb and Cr
};

struct alignas(16) LayerParams {
    std::array<float, 4> inv0{};  // output -> source: a, b, tx (x' = a x + b y + tx)
    std::array<float, 4> inv1{};  // c, d, ty
    std::array<float, 4> crop{};  // source pixels: x0, y0, x1, y1 (half-open)
    std::array<float, 4> yuv_r{}; // R' = dot(yuv_r.xyz, yuv) + yuv_r.w
    std::array<float, 4> yuv_g{};
    std::array<float, 4> yuv_b{};
    std::array<float, 4> gamut_r{}; // linear source primaries -> BT.709 rows
    std::array<float, 4> gamut_g{};
    std::array<float, 4> gamut_b{};
    std::array<float, 4> misc{};          // opacity, sample scale
    std::array<std::int32_t, 4> mode{};   // chroma mode, chroma shift x, chroma shift y, transfer
    std::array<std::int32_t, 4> extra{};  // blend mode, source width, source height
    std::array<std::int32_t, 4> region{}; // output pixels to process: x0, y0, x1, y1
    // The look (src/look.hpp): gains r, g, b and the contrast power; matrix rows with offsets;
    // vignette darkening, its center (source pixels) and 1 / the distance to the corners.
    std::array<float, 4> gains{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> look_r{1.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> look_g{0.0F, 1.0F, 0.0F, 0.0F};
    std::array<float, 4> look_b{0.0F, 0.0F, 1.0F, 0.0F};
    std::array<float, 4> vignette{};
    // Blur and sharpen (src/look.hpp Detail): mode, sharpen amount, radius in taps, reduction
    // factor; then the gaussian weights, center first, four per vector.
    std::array<float, 4> detail{};
    std::array<std::array<float, 4>, (kMaxBlurRadius + 4) / 4> weights{};
    // Grading (grade.hpp): which stages run (GradeStage bits) and the LUT size; the LUT amount;
    // the CDL (saturation in slope.w); the LUT domain minimum and (size - 1) / its extent; the
    // point counts of the master, red, green and blue curves; and their points (x, y, tangent),
    // kMaxCurvePoints per curve.
    std::array<std::int32_t, 4> grade{};
    std::array<float, 4> lut_amount{};
    std::array<float, 4> cdl_slope{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> cdl_offset{};
    std::array<float, 4> cdl_power{1.0F, 1.0F, 1.0F, 0.0F};
    std::array<float, 4> lut_min{};
    std::array<float, 4> lut_scale{};
    std::array<std::int32_t, 4> curve_count{};
    std::array<std::array<float, 4>, 4 * kMaxCurvePoints> curve{};
};
static_assert(sizeof(LayerParams) == 96U * 16U, "LayerParams must match the std140 block");

// Bits of LayerParams::grade[0].
enum GradeStage : std::int32_t {
    kGradeCdl = 1,
    kGradeCurves = 2,
    kGradeLut = 4,
};

struct PreparedLayer {
    LayerParams params;
    bool visible = false; // false when the layer covers no output pixel
};

// Resolves geometry, color and sampling for one layer. Errors for unsupported sample layouts.
[[nodiscard]] Result<PreparedLayer>
prepare_layer(const Layer& layer, const LayerInput& input, const media::SampleLayout& layout,
              std::uint32_t source_width, std::uint32_t source_height, std::uint32_t out_width,
              std::uint32_t out_height);

// Premultiplied linear background from the graph's straight-alpha color.
[[nodiscard]] std::array<float, 4> premultiplied_background(const RenderGraph& graph) noexcept;

} // namespace oma::compositor
