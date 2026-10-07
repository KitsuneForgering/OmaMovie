#include "video_scheduler.hpp"

#include "oma/gpu/device.hpp"

#include <utility>

VideoScheduler::~VideoScheduler() {
    stop();
    pipeline_.shutdown();
}

void VideoScheduler::start(std::shared_ptr<const oma::timeline::Timeline> timeline,
                           std::shared_ptr<const MediaPaths> paths, std::shared_ptr<const LutTables> luts,
                           std::uint32_t width, std::uint32_t height,
                           std::int64_t ticks_per_frame, std::int64_t from, int step, std::int64_t last) {
    stop();
    step_ = step == 0 ? 1 : step;
    target_.store(from, std::memory_order_relaxed);
    auto queue = std::make_shared<oma::BoundedQueue<Frame>>(kAhead);
    queue_ = queue;
    const int stride = step_;
    producer_ = pipeline_.submit("playback-video", [this, queue, timeline = std::move(timeline),
                                                    paths = std::move(paths), luts = std::move(luts), width, height,
                                                    ticks_per_frame,
                                                    from, stride, last](oma::JobContext& ctx) {
        std::int64_t next = from;
        while (!ctx.is_cancelled()) {
            // Never prepare a frame the clock has already passed.
            const std::int64_t clock = target_.load(std::memory_order_relaxed);
            if (stride > 0 ? next < clock : next > clock) {
                next = clock;
            }
            if (next < 0 || next > last) {
                break; // the end in this direction; the clock's owner stops playback
            }
            if (device_ != nullptr && !device_->admit([&] { return ctx.is_cancelled(); })) {
                break; // cancelled while Qt held admission
            }
            auto view = build_viewer_frame(*timeline, *paths, *luts, width, height, next, ticks_per_frame, frames_);
            if (device_ != nullptr) device_->leave();
            if (!view) {
                const std::scoped_lock lock(error_mutex_);
                error_ = view.error().summary();
                break;
            }
            if (!queue->push(Frame{.frame = next, .view = std::move(*view)})) {
                break; // closed: stopped
            }
            next += stride;
        }
        return oma::Result<void>{};
    });
}

void VideoScheduler::set_device(const oma::gpu::Device* device) {
    if (device == device_) return;
    stop();
    // The old decoders (and any GPU frames they own) go before the new source is used.
    frames_ = FrameSource(device);
    device_ = device;
}

void VideoScheduler::stop() noexcept {
    if (queue_) {
        queue_->close(); // releases a producer blocked on a full queue
    }
    if (producer_.valid()) {
        producer_.cancel();
        (void)producer_.wait();
        producer_ = {};
    }
    queue_.reset();
}

std::optional<VideoScheduler::Frame> VideoScheduler::take(std::int64_t target) {
    target_.store(target, std::memory_order_relaxed);
    std::optional<Frame> shown;
    if (!queue_) {
        return shown;
    }
    while (auto f = queue_->try_pop_if([&](const Frame& candidate) { return not_after(candidate.frame, target); })) {
        if (shown) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
        }
        shown = std::move(f);
    }
    return shown;
}

std::optional<std::string> VideoScheduler::take_error() {
    const std::scoped_lock lock(error_mutex_);
    return std::exchange(error_, std::nullopt);
}
