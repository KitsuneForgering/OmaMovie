// Slip (new source range, same place) and slide (same source range, the neighbours give or take
// the frames): by keyboard on the selected clip or by Alt / Alt+Shift dragging a storyline clip.
#include "session.hpp"

#include "oma/timeline/edit.hpp"

namespace tl = oma::timeline;

namespace {
std::unique_ptr<tl::Command> shift(const tl::Timeline& t, tl::ClipId clip, bool slide,
                                   std::int64_t ticks) {
    if (t.find_clip(clip) == nullptr || ticks == 0)
        return nullptr;
    return slide ? tl::edit::slide(clip, t.at(ticks)) : tl::edit::slip(clip, t.at(ticks));
}
} // namespace

void Session::previewSlip(double id, bool slide, int frames) {
    const TrimPreview key{.clip = id,
                          .frames = frames,
                          .edit = slide ? EdgeEdit::Slide : EdgeEdit::Slip,
                          .trim = false,
                          .revision = editor_ ? editor_->revision() : 0};
    if (trim_preview_ == key)
        return;
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    previewCommand(editor_ ? shift(editor_->timeline(), clip, slide, frames * ticksPerFrame())
                           : nullptr);
    trim_preview_ = key;
}

void Session::slipClip(double id, bool slide, int frames) {
    clearEditScope();
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    if (editor_ && run(shift(editor_->timeline(), clip, slide, frames * ticksPerFrame())))
        showEdge(slide ? EdgeEdit::Slide : EdgeEdit::Slip, clip);
}

void Session::nudgeSlip(int frames) {
    slipClip(static_cast<double>(selected_clip_.value()), false, frames);
}

void Session::nudgeSlide(int frames) {
    slipClip(static_cast<double>(selected_clip_.value()), true, frames);
}
