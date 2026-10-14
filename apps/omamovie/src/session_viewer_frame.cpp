// Paused viewer frames: one request in flight and only the latest wish kept (Session::wanted_);
// a newer request stops the one in flight between decoded frames.
#include "session.hpp"

#include "preview_item.hpp"
#include "ui_post.hpp"
#include "viewer_decoder.hpp"

#include <chrono>

namespace {
std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
} // namespace

void Session::requestFrame() {
    if (playing()) return; // the scheduler feeds the viewer while playing
    wanted_ = playhead_;
    viewer_requests_.fetch_add(1, std::memory_order_relaxed); // supersedes a frame in flight
    if (busy_ && !superseded_) { // timed from the first request that replaces the one in flight
        superseded_ = true;
        superseded_ns_.store(now_ns(), std::memory_order_relaxed);
    }
    if (!busy_) submitFrame();
}

bool Session::hardwarePreview() const {
    return preview_ != nullptr && preview_->hardwareDecodeAvailable();
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
    superseded_ = false;
    const unsigned generation = generation_;
    const std::uint64_t request = viewer_requests_.load(std::memory_order_relaxed);
    // Hardware decoding when the viewer uses it; the device then also tells a lost GPU apart.
    const oma::gpu::Device* device = hardwarePreview() && hardware_frames_ ? preview_->device() : nullptr;
    frame_job_ = viewer_pool_.submit("viewer-frame", [this, generation, request, frame, device, timeline = shown,
                                                  paths = paths_, luts = luts_, width = canvas_width_,
                                                  height = canvas_height_, tpf](oma::JobContext& ctx) {
        // Latest request wins: a newer request, or the session shutting down, stops this frame's
        // seek between decoded frames instead of a GOP of catch-up nobody will see.
        const auto wanted = [&] {
            return !ctx.is_cancelled() && viewer_requests_.load(std::memory_order_relaxed) == request;
        };
        auto view = decode_paused_frame(device != nullptr ? *hardware_frames_ : viewer_frames_, device, wanted, *timeline,
                                        *paths, *luts, width, height, frame, tpf);
        const bool superseded = !view && view.error().code() == oma::ErrorCode::Cancelled && !ctx.is_cancelled();
        const double stop_ms =
            superseded ? static_cast<double>(now_ns() - superseded_ns_.load(std::memory_order_relaxed)) / 1e6 : -1.0;
        post_to_ui(this, [this, generation, request, stop_ms, view = std::move(view)]() mutable {
            busy_ = false;
            if (stop_ms >= 0 && generation == generation_ && cancel_latencies_.size() < 4096)
                cancel_latencies_.push_back(stop_ms); // a project switch is not a scrub supersede
            const bool current = request == viewer_requests_.load(std::memory_order_relaxed);
            if (generation != generation_) {
                // Another project's frame: dropped, but a request the new one made meanwhile still runs.
            } else if (!view && view.error().code() == oma::ErrorCode::Cancelled) {
                // Superseded: the newer request is waiting in wanted_.
            } else if (!view && view.error().code() == oma::ErrorCode::DeviceLost) {
                if (preview_ != nullptr) preview_->setFrame(nullptr); // blank; the viewer reports the loss
            } else if (!view) {
                fail(QString::fromStdString(view.error().message()));
            } else if (current && preview_ != nullptr && !playing()) {
                preview_->setFrame(std::move(*view)); // never a frame a newer request replaced
            }
            if (wanted_)
                submitFrame();
        });
        return oma::Result<void>{};
    });
}

std::vector<double> Session::takeCancelLatencies() { return std::exchange(cancel_latencies_, {}); }
