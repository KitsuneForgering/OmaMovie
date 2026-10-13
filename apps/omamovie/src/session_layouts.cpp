// Presets (M8): a connected clip laid out as picture-in-picture or as the right half of a split
// screen, and built-in effect combinations. Each is one set_video edit of ordinary values, so the
// result stays editable in its drawer and undoes in one step.
#include "session.hpp"

#include "oma/timeline/effect_presets.hpp"

namespace tl = oma::timeline;

void Session::applyLayout(int preset) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const tl::Track* track = c != nullptr ? editor_->timeline().track_of(c->id) : nullptr;
    if (track == nullptr || track->id == primary_ || track->kind != tl::TrackKind::Video) {
        setNotice(QStringLiteral("Select a clip above the storyline to lay it out"));
        return;
    }
    const double w = canvas_width_;
    const double h = canvas_height_;
    tl::VideoProperties video = c->video;
    video.transform_keys.clear(); // a layout is a still placement; keys would move it away
    video.fit = tl::Fit::Fit;
    if (preset == 0) {
        // A third of the frame in the lower right corner, 4 % of the width from the edges.
        constexpr double kScale = 1.0 / 3.0;
        const double margin = 0.04 * w;
        video.crop = {};
        video.transform = {.offset_x = (w * (1.0 - kScale) / 2.0) - margin,
                           .offset_y = (h * (1.0 - kScale) / 2.0) - margin,
                           .scale_x = kScale,
                           .scale_y = kScale,
                           .rotation = 0.0};
    } else {
        // The middle half of the picture in the right half of the frame; the storyline shows
        // on the left and keeps its own framing.
        video.crop = {.left = 0.25, .top = 0.0, .right = 0.25, .bottom = 0.0};
        video.transform = {
            .offset_x = w / 4.0, .offset_y = 0.0, .scale_x = 1.0, .scale_y = 1.0, .rotation = 0.0};
    }
    setSelectedVideo(video);
}

QVariantList Session::effectPresets() const {
    QVariantList out;
    for (const tl::EffectPreset& p : tl::effect_presets())
        out.push_back(QVariantMap{
            {"id", QString::fromUtf8(p.id.data(), static_cast<qsizetype>(p.id.size()))},
            {"name", QString::fromUtf8(p.name.data(), static_cast<qsizetype>(p.name.size()))}});
    return out;
}

void Session::applyEffectPreset(const QString& id) {
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    const tl::EffectPreset* preset = tl::find_effect_preset(id.toStdString());
    if (c == nullptr || preset == nullptr || c->title)
        return;
    tl::VideoProperties video = c->video;
    video.effects = tl::with_preset(std::move(video.effects), *preset);
    setSelectedVideo(video);
}
