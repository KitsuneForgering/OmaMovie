#include "oma/compositor/render_graph.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace oma::compositor {

namespace {

constexpr std::uint32_t kMaxDimension = 16384;

Error invalid(std::string message, std::string context = {}) {
    return {ErrorCode::InvalidArgument, Category::Compositor, std::move(message),
            std::move(context)};
}

bool valid_fraction(double v) {
    return std::isfinite(v) && v >= 0.0 && v < 1.0;
}

} // namespace

Result<void> validate(const RenderGraph& graph, std::size_t input_count) {
    if (graph.width == 0 || graph.height == 0 || graph.width > kMaxDimension ||
        graph.height > kMaxDimension) {
        return std::unexpected(
            invalid("output size out of range", std::format("{}x{}", graph.width, graph.height)));
    }
    for (std::size_t i = 0; i < graph.layers.size(); ++i) {
        const Layer& l = graph.layers[i];
        const std::string where = std::format("layer {}", i);
        if (l.input >= input_count) {
            return std::unexpected(invalid("layer refers to a missing input", where));
        }
        const Crop& c = l.crop;
        if (!valid_fraction(c.left) || !valid_fraction(c.top) || !valid_fraction(c.right) ||
            !valid_fraction(c.bottom) || c.left + c.right >= 1.0 || c.top + c.bottom >= 1.0) {
            return std::unexpected(invalid("crop removes the whole source", where));
        }
        const Transform& t = l.transform;
        if (!std::isfinite(t.offset_x) || !std::isfinite(t.offset_y) ||
            !std::isfinite(t.rotation) || !std::isfinite(t.scale_x) || !std::isfinite(t.scale_y) ||
            t.scale_x == 0.0 || t.scale_y == 0.0) {
            return std::unexpected(invalid("degenerate transform", where));
        }
        if (std::isnan(l.opacity) || l.opacity < 0.0F || l.opacity > 1.0F) {
            return std::unexpected(invalid("opacity outside [0, 1]", where));
        }
        const ColorAdjust& a = l.color;
        const auto within = [](double v, double limit) {
            return std::isfinite(v) && std::abs(v) <= limit;
        };
        if (!within(a.exposure, 4.0) || !within(a.contrast, 1.0) || !within(a.saturation, 1.0) ||
            !within(a.temperature, 1.0)) {
            return std::unexpected(invalid("color adjustment out of range", where));
        }
        if (!std::isfinite(l.filter.amount) || l.filter.amount < 0.0 || l.filter.amount > 1.0 ||
            l.filter.kind > FilterKind::Vignette) {
            return std::unexpected(invalid("invalid filter", where));
        }
        if (!within(l.sharpness, 1.0)) {
            return std::unexpected(invalid("sharpness outside [-1, 1]", where));
        }
    }
    return {};
}

Affine source_to_output(const Layer& layer, const SourceGeometry& source, std::uint32_t out_width,
                        std::uint32_t out_height) {
    const auto w = static_cast<double>(source.width);
    const auto h = static_cast<double>(source.height);
    const Crop& c = layer.crop;
    const double crop_w = w * (1.0 - c.left - c.right);
    const double crop_h = h * (1.0 - c.top - c.bottom);
    const double center_x = (w * c.left) + (crop_w / 2.0);
    const double center_y = (h * c.top) + (crop_h / 2.0);

    const double sar =
        source.sample_aspect.is_positive() ? source.sample_aspect.to_double_approx() : 1.0;
    const bool quarter = ((source.rotation % 180) + 180) % 180 == 90;
    const double shown_w = quarter ? crop_h : crop_w * sar;
    const double shown_h = quarter ? crop_w * sar : crop_h;

    const auto ow = static_cast<double>(out_width);
    const auto oh = static_cast<double>(out_height);
    double fx = 1.0;
    double fy = 1.0;
    switch (layer.fit) {
    case Fit::Fit:
        fx = fy = std::min(ow / shown_w, oh / shown_h);
        break;
    case Fit::Fill:
        fx = fy = std::max(ow / shown_w, oh / shown_h);
        break;
    case Fit::Stretch:
        fx = ow / shown_w;
        fy = oh / shown_h;
        break;
    case Fit::Native:
        break;
    }

    const Transform& t = layer.transform;
    return Affine::translate(-center_x, -center_y)
        .then(Affine::scale(sar, 1.0))
        .then(Affine::rotate(-static_cast<double>(source.rotation))) // display rotation is CCW
        .then(Affine::scale(fx, fy))
        .then(Affine::scale(t.scale_x, t.scale_y))
        .then(Affine::rotate(t.rotation))
        .then(Affine::translate((ow / 2.0) + t.offset_x, (oh / 2.0) + t.offset_y));
}

std::optional<std::array<std::uint32_t, 4>> covered_pixels(const Layer& layer,
                                                           const SourceGeometry& source,
                                                           std::uint32_t out_width,
                                                           std::uint32_t out_height) {
    const Affine m = source_to_output(layer, source, out_width, out_height);
    const auto w = static_cast<double>(source.width);
    const auto h = static_cast<double>(source.height);
    const Crop& c = layer.crop;
    const std::array<std::array<double, 2>, 4> corners{
        {{w * c.left, h * c.top},
         {w * (1.0 - c.right), h * c.top},
         {w * c.left, h * (1.0 - c.bottom)},
         {w * (1.0 - c.right), h * (1.0 - c.bottom)}}};
    double x0 = std::numeric_limits<double>::max();
    double y0 = x0;
    double x1 = std::numeric_limits<double>::lowest();
    double y1 = x1;
    for (const auto& p : corners) {
        const auto q = m.apply(p[0], p[1]);
        x0 = std::min(x0, q[0]);
        y0 = std::min(y0, q[1]);
        x1 = std::max(x1, q[0]);
        y1 = std::max(y1, q[1]);
    }
    const auto clip = [](double v, std::uint32_t hi) {
        return static_cast<std::uint32_t>(std::clamp(v, 0.0, static_cast<double>(hi)));
    };
    const std::uint32_t ix0 = clip(std::floor(x0), out_width);
    const std::uint32_t iy0 = clip(std::floor(y0), out_height);
    const std::uint32_t ix1 = clip(std::ceil(x1), out_width);
    const std::uint32_t iy1 = clip(std::ceil(y1), out_height);
    if (ix0 >= ix1 || iy0 >= iy1) {
        return std::nullopt;
    }
    return std::array<std::uint32_t, 4>{ix0, iy0, ix1, iy1};
}

} // namespace oma::compositor
