// Presets (M8): a connected clip laid out as picture-in-picture or as the right half of a split
// screen (the storyline under it framed into the left half), and built-in effect combinations.
// Each is one edit of ordinary values, so the result stays editable in its drawer and undoes in
// one step.
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
    if (preset == 0) {
        tl::VideoProperties video = c->video;
        video.transform_keys.clear(); // a layout is a still placement; keys would move it away
        video.fit = tl::Fit::Fit;
        // A third of the frame in the lower right corner, 4 % of the width from the edges.
        constexpr double kScale = 1.0 / 3.0;
        const double margin = 0.04 * w;
        video.crop = {};
        video.transform = {.offset_x = (w * (1.0 - kScale) / 2.0) - margin,
                           .offset_y = (h * (1.0 - kScale) / 2.0) - margin,
                           .scale_x = kScale,
                           .scale_y = kScale,
                           .rotation = 0.0};
        setSelectedVideo(video);
        return;
    }
    // Split screen: the middle half of each picture side by side, the clip on the right and the
    // storyline under it on the left. Storyline clips are split at the clip's edges so their
    // framing changes only while the split screen shows.
    const auto half = [w](tl::VideoProperties v, double side) {
        v.transform_keys.clear(); // a layout is a still placement; keys would move it away
        v.fit = tl::Fit::Fit;
        v.crop = {.left = 0.25, .top = 0.0, .right = 0.25, .bottom = 0.0};
        v.transform = {.offset_x = side * w / 4.0,
                       .offset_y = 0.0,
                       .scale_x = 1.0,
                       .scale_y = 1.0,
                       .rotation = 0.0};
        return v;
    };
    const tl::Timeline& t = editor_->timeline();
    std::vector<std::unique_ptr<tl::Command>> steps;
    steps.push_back(tl::edit::set_video(c->id, half(c->video, 1.0)));
    for (const tl::Clip& under : t.find_track(primary_)->clips) {
        if (under.end_ticks() <= c->start_ticks() || under.start_ticks() >= c->end_ticks() ||
            under.title)
            continue;
        steps.push_back(tl::edit::set_video_range(under.id, t.at(c->start_ticks()),
                                                  t.at(c->end_ticks()), half(under.video, -1.0)));
    }
    run(tl::edit::transaction("Split Screen", std::move(steps)));
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
