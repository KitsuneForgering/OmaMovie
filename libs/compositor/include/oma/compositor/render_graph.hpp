#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/compositor/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// The render graph (CLAUDE.md §9.1): a declarative description of one output frame, produced
// by the timeline for an instant. Plain data, no GPU types; the compositor executes it.

namespace oma::compositor {

// How a source is placed in the output before the layer's own transform.
enum class Fit : std::uint8_t {
    Fit,     // whole source visible, letterboxed (iMovie's default)
    Fill,    // output covered, source cropped
    Stretch, // both axes scaled independently
    Native,  // one source pixel per output pixel
};

enum class BlendMode : std::uint8_t {
    Normal,
    Add,
    Multiply,
    Screen,
};

// Fractions of the source removed from each edge, in [0, 1).
struct Crop {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

struct Transform {
    double offset_x = 0.0; // output pixels from the output center
    double offset_y = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double rotation = 0.0; // degrees, clockwise
};

// Color adjustments of a layer (ui-design §6, Color), applied to the source in linear light
// before blending, in this order: white balance and exposure, contrast, saturation.
struct ColorAdjust {
    double exposure = 0.0;    // stops, in [-4, 4]
    double contrast = 0.0;    // in [-1, 1]: a power of 2^contrast around 18% grey
    double saturation = 0.0;  // in [-1, 1]: -1 grey, 0 unchanged, 1 twice the chroma
    double temperature = 0.0; // in [-1, 1]: cooler (bluer) to warmer, luminance kept
};

// A clip filter (ui-design §6, Effects): a look applied after the color adjustments.
enum class FilterKind : std::uint8_t {
    None,
    BlackAndWhite,
    Sepia,
    Vintage, // faded, warm and less saturated
    Cool,
    Warm,
    Vignette, // darkened corners
};

struct Filter {
    FilterKind kind = FilterKind::None;
    double amount = 1.0; // in [0, 1]: blend between the original and the full look
};

struct Layer {
    std::size_t input = 0; // index into the inputs given to the compositor
    Fit fit = Fit::Fit;
    Crop crop;
    Transform transform;
    float opacity = 1.0F;
    BlendMode blend = BlendMode::Normal;
    ColorAdjust color;
    Filter filter;
    // In [-1, 1]: below 0 a gaussian blur (up to 2% of the source height in standard
    // deviation), above 0 an unsharp mask that sharpens edges; 0 leaves detail alone.
    double sharpness = 0.0;
};

struct RenderGraph {
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    std::array<float, 4> background{0.0F, 0.0F, 0.0F, 1.0F}; // linear RGBA, straight alpha
    std::vector<Layer> layers;                               // bottom first
};

// Checks sizes, input indices, crops, scales, opacity, color adjustments and filters.
[[nodiscard]] Result<void> validate(const RenderGraph& graph, std::size_t input_count);

// What the compositor knows about a source when it renders.
struct SourceGeometry {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    int rotation = 0; // counterclockwise display rotation (0, 90, 180, 270)
    Rational sample_aspect = Rational::literal(1, 1);
};

// Maps a layer from source pixels to output pixels: crop, display rotation and pixel aspect,
// fit, then the layer transform around the output center.
[[nodiscard]] Affine source_to_output(const Layer& layer, const SourceGeometry& source,
                                      std::uint32_t out_width, std::uint32_t out_height);

// Output pixels covered by a layer, clipped to the output: {x0, y0, x1, y1}, half-open.
// std::nullopt when the layer is entirely outside.
[[nodiscard]] std::optional<std::array<std::uint32_t, 4>>
covered_pixels(const Layer& layer, const SourceGeometry& source, std::uint32_t out_width,
               std::uint32_t out_height);

} // namespace oma::compositor
