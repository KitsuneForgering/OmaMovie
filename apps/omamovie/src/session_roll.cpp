// Rolling a cut: one clip's end and the next one's start move together, the sequence keeps its
// length. By keyboard (the selected clip's tail) or by Alt+dragging either side of the cut.
#include "session.hpp"

#include "oma/timeline/edit.hpp"

namespace tl = oma::timeline;

// The clip that ends at the cut on `clip`'s head (the clip just before it, when they touch) or
// tail (the clip itself).
std::optional<tl::ClipId> Session::cutOwner(tl::ClipId clip, bool head) const {
    const tl::Track* track = editor_ ? editor_->timeline().track_of(clip) : nullptr;
    if (track == nullptr)
        return std::nullopt;
    const auto it = std::ranges::find(track->clips, clip, &tl::Clip::id);
    if (!head)
        return it->id;
    if (it == track->clips.begin() || std::prev(it)->end_ticks() != it->start_ticks())
        return std::nullopt;
    return std::prev(it)->id;
}

std::unique_ptr<tl::Command> Session::rollCommand(tl::ClipId clip, bool head, int frames) const {
    const auto owner = frames != 0 ? cutOwner(clip, head) : std::nullopt;
    if (!owner)
        return nullptr;
    const tl::Timeline& t = editor_->timeline();
    const std::int64_t cut = t.find_clip(*owner)->end_ticks();
    return tl::edit::roll(*owner, t.at(cut + static_cast<std::int64_t>(frames) * ticksPerFrame()));
}

void Session::previewRoll(double id, bool head, int frames) {
    const TrimPreview key{.clip = id,
                          .head = head,
                          .frames = frames,
                          .roll = true,
                          .revision = editor_ ? editor_->revision() : 0};
    if (trim_preview_ == key)
        return;
    previewCommand(rollCommand(tl::ClipId(static_cast<std::uint64_t>(id)), head, frames));
    trim_preview_ = key;
}

void Session::rollCut(double id, bool head, int frames) {
    clearEditScope();
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    const auto owner = cutOwner(clip, head);
    if (owner && run(rollCommand(clip, head, frames)))
        showEdge(EdgeEdit::Roll, *owner);
}

void Session::nudgeRoll(int frames) {
    if (selected_clip_.valid())
        rollCut(static_cast<double>(selected_clip_.value()), false, frames);
}
