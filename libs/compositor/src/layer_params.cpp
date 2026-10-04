#include "layer_params.hpp"

#include "look.hpp"

#include "oma/compositor/color.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace oma::compositor {

namespace {

std::array<float, 4> row(const Mat3& m, int r, double w = 0.0) {
    return {static_cast<float>(m.at(r, 0)), static_cast<float>(m.at(r, 1)),
            static_cast<float>(m.at(r, 2)), static_cast<float>(w)};
}

Error unsupported(std::string message) {
    return {ErrorCode::Unsupported, Category::Compositor, std::move(message)};
}

} // namespace

Result<PreparedLayer> prepare_layer(const Layer& layer, const LayerInput& input,
                                    const media::SampleLayout& layout, std::uint32_t source_width,
                                    std::uint32_t source_height, std::uint32_t out_width,
                                    std::uint32_t out_height) {
    if (!layout.yuv) {
        return std::unexpected(unsupported("RGB sources are not supported yet"));
    }
    if (layout.planes < 2 || layout.planes > 3 ||
        (layout.container_bits != 8 && layout.container_bits != 16)) {
        return std::unexpected(unsupported("unsupported sample layout"));
    }

    PreparedLayer out;
    const SourceGeometry geometry{.width = source_width,
                                  .height = source_height,
                                  .rotation = input.rotation,
                                  .sample_aspect = input.sample_aspect};
    const auto inverse = source_to_output(layer, geometry, out_width, out_height).inverse();
    const auto region = covered_pixels(layer, geometry, out_width, out_height);
    if (!inverse || !region || layer.opacity <= 0.0F) {
        return out; // nothing to draw
    }
    out.visible = true;
    LayerParams& p = out.params;
    p.inv0 = {static_cast<float>(inverse->a), static_cast<float>(inverse->b),
              static_cast<float>(inverse->tx), 0.0F};
    p.inv1 = {static_cast<float>(inverse->c), static_cast<float>(inverse->d),
              static_cast<float>(inverse->ty), 0.0F};
    const auto w = static_cast<double>(source_width);
    const auto h = static_cast<double>(source_height);
    p.crop = {static_cast<float>(w * layer.crop.left), static_cast<float>(h * layer.crop.top),
              static_cast<float>(w * (1.0 - layer.crop.right)),
              static_cast<float>(h * (1.0 - layer.crop.bottom))};

    const int height = static_cast<int>(source_height);
    const YuvToRgb yuv = yuv_to_rgb(input.color, layout.bit_depth, height);
    p.yuv_r = row(yuv.matrix, 0, yuv.offset[0]);
    p.yuv_g = row(yuv.matrix, 1, yuv.offset[1]);
    p.yuv_b = row(yuv.matrix, 2, yuv.offset[2]);
    const Mat3 gamut = primaries_to_bt709(input.color.primaries, height);
    p.gamut_r = row(gamut, 0);
    p.gamut_g = row(gamut, 1);
    p.gamut_b = row(gamut, 2);

    // UNORM sampling yields raw / (2^container - 1); rescale to sample / (2^bits - 1), dropping
    // the padding below MSB-aligned samples (P010).
    const double container_max = std::ldexp(1.0, layout.container_bits) - 1.0;
    const double sample_max =
        (std::ldexp(1.0, layout.bit_depth) - 1.0) * std::ldexp(1.0, layout.lsb_shift);
    p.misc = {layer.opacity, static_cast<float>(container_max / sample_max), 0.0F, 0.0F};

    p.mode = {static_cast<std::int32_t>(layout.interleaved_chroma ? ChromaMode::Interleaved
                                                                  : ChromaMode::Planar),
              layout.chroma_shift_x, layout.chroma_shift_y,
              static_cast<std::int32_t>(resolve_transfer(input.color.transfer))};
    p.extra = {static_cast<std::int32_t>(layer.blend), static_cast<std::int32_t>(source_width),
               static_cast<std::int32_t>(source_height), 0};
    p.region = {static_cast<std::int32_t>((*region)[0]), static_cast<std::int32_t>((*region)[1]),
                static_cast<std::int32_t>((*region)[2]), static_cast<std::int32_t>((*region)[3])};

    const Look look = make_look(layer.color, layer.filter);
    p.gains = {static_cast<float>(look.gains[0]), static_cast<float>(look.gains[1]),
               static_cast<float>(look.gains[2]), static_cast<float>(look.power)};
    const auto look_row = [&](std::size_t r) {
        const auto& m = look.matrix[r];
        return std::array<float, 4>{static_cast<float>(m[0]), static_cast<float>(m[1]),
                                    static_cast<float>(m[2]), static_cast<float>(m[3])};
    };
    p.look_r = look_row(0);
    p.look_g = look_row(1);
    p.look_b = look_row(2);
    if (look.vignette > 0.0) {
        // Distances are measured in the cropped source, normalized to 1 at its corners.
        const double x0 = w * layer.crop.left;
        const double y0 = h * layer.crop.top;
        const double x1 = w * (1.0 - layer.crop.right);
        const double y1 = h * (1.0 - layer.crop.bottom);
        const double cx = (x0 + x1) / 2.0;
        const double cy = (y0 + y1) / 2.0;
        const double corner = std::hypot(x1 - cx, y1 - cy);
        p.vignette = {static_cast<float>(look.vignette), static_cast<float>(cx),
                      static_cast<float>(cy), static_cast<float>(1.0 / std::max(corner, 1.0))};
    }
    return out;
}

std::array<float, 4> premultiplied_background(const RenderGraph& graph) noexcept {
    const auto& c = graph.background;
    return {c[0] * c[3], c[1] * c[3], c[2] * c[3], c[3]};
}

} // namespace oma::compositor
