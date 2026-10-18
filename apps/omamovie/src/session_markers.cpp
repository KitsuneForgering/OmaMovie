// Markers on the sequence (M8): M toggles one at the playhead, the previous/next commands jump
// between them. Every change is a timeline command, so markers undo, save and ripple with the
// clips like any edit.
#include "session.hpp"

#include "oma/timeline/edit.hpp"

#include <algorithm>

namespace tl = oma::timeline;

QVariantList Session::markers() const {
    QVariantList out;
    if (!editor_)
        return out;
    for (const tl::Marker& m : editor_->timeline().markers()) {
        out.push_back(QVariantMap{{"id", static_cast<double>(m.id.value())},
                                  {"time", m.time.seconds_approx()},
                                  {"name", QString::fromStdString(m.name)}});
    }
    return out;
}

void Session::toggleMarker() {
    if (!editor_)
        return;
    const tl::Timeline& t = editor_->timeline();
    const auto here = std::ranges::find_if(t.markers(), [&](const tl::Marker& m) {
        return t.to_ticks(m.time).value_or(-1) == playhead_;
    });
    if (here != t.markers().end()) {
        run(tl::edit::remove_marker(here->id));
        return;
    }
    const auto name = "Marker " + std::to_string(t.markers().size() + 1);
    run(tl::edit::add_marker(editor_->new_marker_id(), t.at(playhead_), name));
}

void Session::seekMarker(int direction) {
    if (!editor_ || direction == 0)
        return;
    const tl::Timeline& t = editor_->timeline();
    std::optional<std::int64_t> best;
    for (const tl::Marker& m : t.markers()) {
        const std::int64_t at = t.to_ticks(m.time).value_or(playhead_);
        const bool ahead = direction > 0 ? at > playhead_ : at < playhead_;
        if (ahead && (!best || (direction > 0 ? at < *best : at > *best)))
            best = at;
    }
    if (best)
        setFrame(*best / ticksPerFrame());
    else
        setNotice(direction > 0 ? QStringLiteral("No marker after the playhead")
                                : QStringLiteral("No marker before the playhead"));
}
