// The selected clip's keyframes by parameter, for the Info drawer's dopesheet. Times are
// sequence seconds for display and seeking only; the keys stay exact in source time.
#include "session.hpp"

namespace tl = oma::timeline;

QVariantList Session::keyTracks() const {
    QVariantList out;
    const tl::Clip* c = editor_ ? editor_->timeline().find_clip(selected_clip_) : nullptr;
    // A ramped or frozen clip has no single source-to-sequence mapping to draw keys with.
    if (c == nullptr || !c->time_map.is_constant())
        return out;
    const double start = c->start.seconds_approx();
    const double end = start + c->duration.seconds_approx();
    const double in = c->source_in.seconds_approx();
    const double speed = c->time_map.speed().to_double_approx();
    const auto row = [&](const QString& name, const auto& keys) {
        QVariantList times;
        for (const auto& k : keys) {
            const double at = start + ((k.at.seconds_approx() - in) / speed);
            if (at >= start && at <= end)
                times.push_back(at);
        }
        if (!times.isEmpty())
            out.push_back(
                QVariantMap{{"name", name}, {"times", times}, {"start", start}, {"end", end}});
    };
    row(QStringLiteral("Transform"), c->video.transform_keys);
    row(QStringLiteral("Opacity"), c->video.opacity_keys);
    row(QStringLiteral("Volume"), c->audio.gain_keys);
    return out;
}
