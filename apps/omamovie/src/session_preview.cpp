// Edit previews: what a command would do (editScope), simulated on a copy of the timeline.
#include "edit_scope.hpp"
#include "session.hpp"

#include "oma/timeline/edit.hpp"

namespace tl = oma::timeline;

void Session::previewDelete(bool ripple) {
    previewCommand(deleteCommand(ripple));
}

void Session::previewTrim(double id, bool head, int frames) {
    // A drag sends several pointer events per snapped frame: simulate each frame once.
    const TrimPreview key{
        .clip = id, .head = head, .frames = frames, .revision = editor_ ? editor_->revision() : 0};
    if (trim_preview_ == key)
        return;
    const tl::ClipId clip(static_cast<std::uint64_t>(id));
    previewCommand(frames == 0 ? nullptr : trimCommand(clip, head, frames));
    trim_preview_ = key;
}

void Session::clearEditScope() {
    trim_preview_.reset();
    if (edit_scope_.isEmpty())
        return;
    edit_scope_.clear();
    emit editScopeChanged();
}

void Session::previewCommand(std::unique_ptr<tl::Command> command) {
    if (!editor_ || !command)
        return clearEditScope();
    // One timeline copy per preview, O(clips): within the long-form gate at 5,000 clips
    // (Docs/Research/evidence/2026-10-12-m6-trim-drag.txt).
    tl::Editor trial{tl::Timeline(editor_->timeline())};
    if (!trial.execute(std::move(command)))
        return clearEditScope();
    edit_scope_ = edit_scope(editor_->timeline(), trial.timeline());
    emit editScopeChanged();
}
