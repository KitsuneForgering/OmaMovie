// Paused viewer frames: one request in flight and only the latest wish kept (Session::wanted_);
// a newer request stops the one in flight between decoded frames.
#include "session.hpp"

#include "preview_item.hpp"
#include "ui_post.hpp"
#include "viewer_frame.hpp"

void Session::requestFrame() {
    if (playing()) return; // the scheduler feeds the viewer while playing
    if (hardwarePreview()) {
        if (!snapshot_) refreshSnapshot();
        requestHardwareFrame(source_index_ >= 0 ? source_frame_ : frame());
        return;
    }
    wanted_ = playhead_;
    viewer_requests_.fetch_add(1, std::memory_order_relaxed); // supersedes a frame in flight
    if (!busy_) submitFrame();
}

bool Session::hardwarePreview() const {
    return preview_ != nullptr && preview_->hardwareDecodeAvailable();
}

void Session::requestHardwareFrame(std::int64_t frame) {
    const auto& shown = source_timeline_ ? source_timeline_ : snapshot_;
    if (preview_ != nullptr && shown) {
        preview_->setRequest(shown, paths_, luts_, canvas_width_, canvas_height_, frame,
                             shown->to_ticks(shown->frame_rate().frame_to_time(1)).value_or(1));
    }
}

void Session::submitFrame() {
    if (!wanted_ || !editor_) return;
    std::int64_t frame = *std::exchange(wanted_, std::nullopt) / ticksPerFrame();
    if (!snapshot_) refreshSnapshot();
    auto shown = snapshot_;
    std::int64_t tpf = ticksPerFrame();
    if (source_timeline_) { // the source viewer: its own timeline and playhead
        shown = source_timeline_;
        frame = source_frame_;
        tpf = shown->to_ticks(shown->frame_rate().frame_to_time(1)).value_or(1);
    }
    busy_ = true;
    const unsigned generation = generation_;
    const std::uint64_t request = viewer_requests_.load(std::memory_order_relaxed);
    frame_job_ = workers_.submit("viewer-frame", [this, generation, request, frame, timeline = shown, paths = paths_,
                                                  luts = luts_, width = canvas_width_, height = canvas_height_,
                                                  tpf](oma::JobContext&) {
        // Scrubbing: a newer request stops this frame's seek between decoded frames instead of
        // waiting for a GOP of catch-up nobody will see (software decode, the paused viewer).
        frames_.set_interrupt([this, request] { return viewer_requests_.load(std::memory_order_relaxed) != request; });
        auto view = build_viewer_frame(*timeline, *paths, *luts, width, height, frame, tpf, frames_);
        frames_.set_interrupt({});
        post_to_ui(this, [this, generation, view = std::move(view)]() mutable {
            busy_ = false;
            if (generation != generation_)
                return;
            if (!view && view.error().code() == oma::ErrorCode::Cancelled) {
                // Superseded: the newer request is waiting in wanted_.
            } else if (!view) {
                fail(QString::fromStdString(view.error().message()));
            } else if (preview_ != nullptr && !playing()) {
                preview_->setFrame(std::move(*view));
            }
            if (wanted_)
                submitFrame();
        });
        return oma::Result<void>{};
    });
}
