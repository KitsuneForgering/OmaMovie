// What the keyboard context menu (Shift+F10, Menu) opens on, decided here so QML only places
// the menu (CLAUDE.md §11.1).
#include "session.hpp"

#include "oma/timeline/evaluate.hpp"

#include <algorithm>
#include <iterator>

namespace tl = oma::timeline;

QVariantMap Session::clipMenuTarget() const {
    const QVariantMap none{{"kind", QStringLiteral("empty")}};
    if (!editor_) return none;
    const tl::Timeline& t = editor_->timeline();
    const tl::Clip* clip = selected_clip_.valid() ? t.find_clip(selected_clip_) : nullptr;
    const tl::Track* track = clip != nullptr ? t.track_of(selected_clip_) : nullptr;
    if (track != nullptr) {
        QVariantMap out{{"start", clip->start.seconds_approx()}, {"title", clip->title.has_value()}};
        if (track->id == primary_) {
            out.insert("kind", QStringLiteral("storyline"));
            return out;
        }
        const bool audio = track->kind == tl::TrackKind::Audio;
        const auto ids = audio ? audioLanes() : lanes(tl::TrackKind::Video);
        out.insert("kind", audio ? QStringLiteral("audio") : QStringLiteral("video"));
        out.insert("lane", static_cast<int>(std::ranges::find(ids, track->id) - ids.begin()));
        return out;
    }
    // Nothing selected: the storyline cut within half a frame of the playhead (not the first
    // clip's start, which joins nothing).
    const tl::Track* storyline = t.find_track(primary_);
    if (storyline == nullptr || storyline->clips.size() < 2) return none;
    const std::int64_t half = ticksPerFrame() / 2;
    const auto it = std::ranges::lower_bound(std::next(storyline->clips.begin()), storyline->clips.end(),
                                             playhead_ - half, {}, &tl::Clip::start_ticks);
    if (it == storyline->clips.end() || it->start_ticks() > playhead_ + half) return none;
    const auto index = static_cast<std::size_t>(it - storyline->clips.begin());
    const auto window = tl::transition_window(t, *storyline, index);
    return {{"kind", QStringLiteral("cut")},
            {"clipId", static_cast<double>(it->id.value())},
            {"transitionKind", it->transition_in && window ? static_cast<int>(window->kind) : -1}};
}
