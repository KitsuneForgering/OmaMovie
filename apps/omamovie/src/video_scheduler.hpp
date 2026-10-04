#pragma once

#include "viewer_frame.hpp"

#include "oma/base/bounded_queue.hpp"
#include "oma/base/jobs.hpp"
#include "oma/timeline/model.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

// Video side of playback (M4 playback scheduler): a pipeline thread decodes and prepares the
// frames ahead of the clock into a bounded queue; the clock's owner takes, at each tick, the
// newest frame not past the clock and drops the older ones. Video follows the master clock by
// dropping or repeating frames (CLAUDE.md §12).
//
// Threading: start/stop/take run on the UI thread. The producer runs on this scheduler's own
// one-thread JobPool (the playback pipeline of ADR-0003) with its own decoders, works on an
// immutable timeline snapshot, and blocks when the queue is full (backpressure). stop() closes
// the queue and joins the producer, so cancellation on seek or speed change is immediate.
class VideoScheduler {
public:
    struct Frame {
        std::int64_t frame = 0;
        std::shared_ptr<const ViewerFrame> view;
    };

    // Frames prepared ahead: about a quarter second at 30 fps, bounded memory.
    static constexpr std::size_t kAhead = 8;

    VideoScheduler() = default;
    ~VideoScheduler();
    VideoScheduler(const VideoScheduler&) = delete;
    VideoScheduler& operator=(const VideoScheduler&) = delete;
    VideoScheduler(VideoScheduler&&) = delete;
    VideoScheduler& operator=(VideoScheduler&&) = delete;

    // Starts preparing frames from `from`, `step` frames apart (2 for double speed, -1 for
    // reverse), up to frame `last` (or 0 in reverse).
    void start(std::shared_ptr<const oma::timeline::Timeline> timeline, std::shared_ptr<const MediaPaths> paths,
               std::shared_ptr<const LutTables> luts, std::uint32_t width, std::uint32_t height, std::int64_t ticks_per_frame, std::int64_t from,
               int step, std::int64_t last);
    void stop() noexcept;

    // The newest prepared frame at or before `target` in the playback direction, if any is new;
    // older prepared frames are dropped. Also tells the producer where the clock is, so a late
    // producer skips ahead instead of preparing frames nobody will show.
    [[nodiscard]] std::optional<Frame> take(std::int64_t target);

    // Prepared frames never shown because a newer one was due.
    [[nodiscard]] std::int64_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }
    // A failure in the producer (undecodable media), reported once.
    [[nodiscard]] std::optional<std::string> take_error();

private:
    [[nodiscard]] bool not_after(std::int64_t frame, std::int64_t target) const noexcept {
        return step_ > 0 ? frame <= target : frame >= target;
    }

    int step_ = 1;
    std::shared_ptr<oma::BoundedQueue<Frame>> queue_;
    std::atomic<std::int64_t> target_{0};
    std::atomic<std::int64_t> dropped_{0};
    std::mutex error_mutex_;
    std::optional<std::string> error_;
    oma::JobHandle producer_;
    oma::JobPool pipeline_{1}; // destroyed first: the producer never outlives what it uses
};
