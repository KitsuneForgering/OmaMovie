#include "viewer_frame.hpp"

#include "oma/timeline/effects.hpp"
#include "oma/timeline/evaluate.hpp"

#include <utility>

namespace tl = oma::timeline;

namespace {

// The timeline mirrors the render graph's enumerations (it may not depend on the compositor,
// CLAUDE.md §5.2); the casts below rely on identical values.
static_assert(static_cast<int>(tl::FilterKind::Vignette) == static_cast<int>(oma::compositor::FilterKind::Vignette));
static_assert(static_cast<int>(tl::FilterKind::BlackAndWhite) ==
              static_cast<int>(oma::compositor::FilterKind::BlackAndWhite));
static_assert(static_cast<int>(tl::BlendMode::Screen) == static_cast<int>(oma::compositor::BlendMode::Screen));
static_assert(static_cast<int>(tl::Fit::Native) == static_cast<int>(oma::compositor::Fit::Native));

std::vector<oma::compositor::CurvePoint> to_points(const std::vector<tl::CurvePoint>& points) {
    std::vector<oma::compositor::CurvePoint> out;
    out.reserve(points.size());
    for (const tl::CurvePoint& p : points) out.push_back({.x = p.x, .y = p.y});
    return out;
}

oma::compositor::Layer to_layer(const tl::VideoProperties& v, std::size_t input, const LutTables& luts) {
    oma::compositor::Layer layer;
    layer.input = input;
    layer.fit = static_cast<oma::compositor::Fit>(v.fit); // same enumerators, same order
    layer.crop = {.left = v.crop.left, .top = v.crop.top, .right = v.crop.right, .bottom = v.crop.bottom};
    layer.transform = {.offset_x = v.transform.offset_x,
                       .offset_y = v.transform.offset_y,
                       .scale_x = v.transform.scale_x,
                       .scale_y = v.transform.scale_y,
                       .rotation = v.transform.rotation};
    layer.opacity = v.opacity;
    layer.blend = static_cast<oma::compositor::BlendMode>(v.blend);
    layer.color = {.exposure = v.color.exposure,
                   .contrast = v.color.contrast,
                   .saturation = v.color.saturation,
                   .temperature = v.color.temperature};
    // Enabled effects this build knows, by stage (ADR-0016); unknown ones are reported, not drawn.
    for (const tl::Effect& e : v.effects) {
        const tl::EffectDefinition* d = e.enabled ? tl::find_effect_definition(e.definition) : nullptr;
        if (d == nullptr) continue;
        const double amount = tl::effect_param(e, "amount");
        if (d->stage == tl::EffectStage::Detail) {
            layer.sharpness = amount;
        } else {
            layer.looks.push_back({.kind = static_cast<oma::compositor::FilterKind>(d->look), // same enumerators
                                   .amount = amount});
        }
    }
    const tl::ColorGrade& g = v.grade;
    layer.grade.cdl = {.slope = g.cdl.slope, .offset = g.cdl.offset, .power = g.cdl.power, .saturation = g.cdl.saturation};
    layer.grade.curves = {.master = to_points(g.curves.master),
                          .red = to_points(g.curves.red),
                          .green = to_points(g.curves.green),
                          .blue = to_points(g.curves.blue)};
    if (const auto lut = luts.find(g.lut.value()); g.lut.valid() && lut != luts.end()) {
        layer.grade.lut = lut->second;
        layer.grade.lut_amount = g.lut_amount;
    }
    return layer;
}

} // namespace

oma::Result<std::shared_ptr<ViewerFrame>> build_viewer_frame(const tl::Timeline& timeline, const MediaPaths& paths,
                                                             const LutTables& luts, std::uint32_t width, std::uint32_t height,
                                                             std::int64_t frame, std::int64_t ticks_per_frame,
                                                             FrameSource& frames) {
    const auto at = timeline.at(frame * ticks_per_frame);
    auto composition = tl::evaluate(timeline, at);
    if (!composition) {
        return std::unexpected(composition.error());
    }
    auto out = std::make_shared<ViewerFrame>();
    out->graph.width = width;
    out->graph.height = height;
    out->frame = frame;
    out->seconds = at.seconds_approx();
    for (const tl::VideoLayer& layer : composition->video) {
        oma::Result<Picture> picture = std::unexpected(oma::Error(oma::ErrorCode::Internal, oma::Category::Ui, ""));
        if (layer.title) {
            picture = frames.title_picture(*layer.title, width, height); // drawn at the canvas size
        } else {
            const auto path = paths.find(layer.media.value());
            if (path == paths.end()) {
                continue; // media not in the library (cannot happen while the library only grows)
            }
            picture = frames.picture_at(path->second, layer.media_time);
        }
        if (!picture) {
            return std::unexpected(picture.error());
        }
        auto composited = to_layer(layer.video, out->pictures.size(), luts);
        composited.opacity *= layer.opacity; // a transition fading it
        composited.reveal = layer.reveal;
        out->graph.layers.push_back(composited);
        out->pictures.push_back(std::move(*picture));
    }
    return out;
}
