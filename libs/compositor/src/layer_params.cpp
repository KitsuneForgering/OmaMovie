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

void prepare_grade(const Grade& g, LayerParams& p) {
    std::int32_t stages = 0;
    if (!g.cdl.identity()) {
        stages |= kGradeCdl;
        for (std::size_t i = 0; i < 3; ++i) {
            p.cdl_slope[i] = static_cast<float>(g.cdl.slope[i]);
            p.cdl_offset[i] = static_cast<float>(g.cdl.offset[i]);
            p.cdl_power[i] = static_cast<float>(g.cdl.power[i]);
        }
        p.cdl_slope[3] = static_cast<float>(g.cdl.saturation);
    }
    if (!g.curves.identity()) {
        stages |= kGradeCurves;
        const std::array<const std::vector<CurvePoint>*, 4> curves{&g.curves.master, &g.curves.red,
                                                                   &g.curves.green, &g.curves.blue};
        for (std::size_t c = 0; c < curves.size(); ++c) {
            const auto& points = *curves[c];
            const auto tangents = curve_tangents(points);
            p.curve_count[c] = static_cast<std::int32_t>(points.size());
            for (std::size_t i = 0; i < points.size(); ++i) {
                p.curve[(c * kMaxCurvePoints) + i] = {static_cast<float>(points[i].x),
                                                      static_cast<float>(points[i].y),
                                                      static_cast<float>(tangents[i]), 0.0F};
            }
        }
    }
    if (g.lut != nullptr && g.lut_amount > 0.0) {
        stages |= kGradeLut;
        const Lut3d& lut = *g.lut;
        p.grade[1] = static_cast<std::int32_t>(lut.size);
        p.lut_amount[0] = static_cast<float>(g.lut_amount);
        for (std::size_t i = 0; i < 3; ++i) {
            p.lut_min[i] = lut.domain_min[i];
            p.lut_scale[i] =
                static_cast<float>(lut.size - 1) / (lut.domain_max[i] - lut.domain_min[i]);
        }
    }
    p.grade[0] = stages;
}

} // namespace

Result<PreparedLayer> prepare_layer(const Layer& layer, const LayerInput& input,
                                    const media::SampleLayout& layout, std::uint32_t source_width,
                                    std::uint32_t source_height, std::uint32_t out_width,
                                    std::uint32_t out_height) {
    if (!layout.yuv && layout.planes != 3) {
        return std::unexpected(unsupported("only planar GBR sources are supported for RGB"));
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
    if (!inverse || !region || layer.opacity <= 0.0F || layer.reveal <= 0.0) {
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
    if (layout.yuv) {
        const YuvToRgb yuv = yuv_to_rgb(input.color, layout.bit_depth, height);
        p.yuv_r = row(yuv.matrix, 0, yuv.offset[0]);
        p.yuv_g = row(yuv.matrix, 1, yuv.offset[1]);
        p.yuv_b = row(yuv.matrix, 2, yuv.offset[2]);
    } else {
        // Planar GBR (still images, RGB codecs): the planes hold G, B, R at full range, so the
        // "YUV" matrix is a permutation and the shader path stays the same.
        p.yuv_r = {0.0F, 0.0F, 1.0F, 0.0F};
        p.yuv_g = {1.0F, 0.0F, 0.0F, 0.0F};
        p.yuv_b = {0.0F, 1.0F, 0.0F, 0.0F};
    }
    // RGB without tagged primaries is sRGB (BT.709 primaries) at any size; untagged video
    // follows the resolution rule in primaries_to_bt709.
    constexpr std::uint8_t kUnspecified = 2;
    constexpr std::uint8_t kBt709 = 1;
    const Mat3 gamut = primaries_to_bt709(
        !layout.yuv && input.color.primaries == kUnspecified ? kBt709 : input.color.primaries,
        height);
    p.gamut_r = row(gamut, 0);
    p.gamut_g = row(gamut, 1);
    p.gamut_b = row(gamut, 2);

    // UNORM sampling yields raw / (2^container - 1); rescale to sample / (2^bits - 1), dropping
    // the padding below MSB-aligned samples (P010).
    const double container_max = std::ldexp(1.0, layout.container_bits) - 1.0;
    const double sample_max =
        (std::ldexp(1.0, layout.bit_depth) - 1.0) * std::ldexp(1.0, layout.lsb_shift);
    // A partial reveal (wipe) stores its edge in output pixels; a negative edge means none.
    const float reveal_edge =
        layer.reveal < 1.0 ? static_cast<float>(layer.reveal * static_cast<double>(out_width))
                           : -1.0F;
    p.misc = {layer.opacity, static_cast<float>(container_max / sample_max), reveal_edge, 0.0F};

    // RGB without a tagged transfer is sRGB (PNG, JPEG); untagged video stays BT.1886.
    constexpr std::uint8_t kSrgb = 13;
    const std::uint8_t transfer =
        !layout.yuv && input.color.transfer == kUnspecified ? kSrgb : input.color.transfer;
    p.mode = {static_cast<std::int32_t>(layout.interleaved_chroma ? ChromaMode::Interleaved
                                                                  : ChromaMode::Planar),
              layout.chroma_shift_x, layout.chroma_shift_y,
              static_cast<std::int32_t>(resolve_transfer(transfer))};
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
    // Source pixels per output pixel, from the area scale of the output -> source mapping.
    const double source_per_output =
        std::sqrt(std::abs((inverse->a * inverse->d) - (inverse->b * inverse->c)));
    const Detail detail = make_detail(layer.sharpness, source_height, source_per_output);
    p.detail = {static_cast<float>(detail.mode), static_cast<float>(detail.amount),
                static_cast<float>(detail.radius), static_cast<float>(detail.factor)};
    for (std::size_t i = 0; i < detail.weights.size(); ++i) {
        p.weights[i / 4][i % 4] = static_cast<float>(detail.weights[i]);
    }
    prepare_grade(layer.grade, p);
    return out;
}

std::array<float, 4> premultiplied_background(const RenderGraph& graph) noexcept {
    const auto& c = graph.background;
    return {c[0] * c[3], c[1] * c[3], c[2] * c[3], c[3]};
}

} // namespace oma::compositor
