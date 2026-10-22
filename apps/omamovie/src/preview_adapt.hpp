#pragma once

// M4 adaptive preview's rule: when more than 2% of the frames over a 120-frame window were
// dropped, the viewer composites at half resolution for the rest of that playback. Each
// playback's first frames are left out: every drop measured on 4K and three-layer scenes fell
// on frames 1-2 as playback started, none later, and counting them halved every 4K playback with
// the default setting (2026-10-22,
// Docs/Research/evidence/2026-10-22-three-layer-playback-audit.md).
#include <cstdint>

class PreviewAdapt {
public:
    static constexpr std::int64_t kSettleFrames = 12; // half a second at 24 fps
    static constexpr std::int64_t kWindowFrames = 120;

    // A playback (or the automatic setting) starts; `dropped` is the scheduler's count now.
    void restart(std::int64_t dropped) noexcept {
        shown_ = -kSettleFrames;
        base_ = dropped;
    }

    // After each shown frame, with the scheduler's dropped count: true when the window that just
    // ended dropped more than 2% of its frames. window_dropped() and window_shown() describe it.
    bool shown(std::int64_t dropped) noexcept {
        if (++shown_ <= 0) {
            base_ = dropped; // still settling: start drops do not count
            return false;
        }
        if (shown_ < kWindowFrames)
            return false;
        window_dropped_ = dropped - base_;
        window_shown_ = shown_;
        shown_ = 0;
        base_ = dropped;
        return window_dropped_ * 50 > window_shown_;
    }

    [[nodiscard]] std::int64_t window_dropped() const noexcept { return window_dropped_; }
    [[nodiscard]] std::int64_t window_shown() const noexcept { return window_shown_; }

private:
    std::int64_t shown_ = -kSettleFrames;
    std::int64_t base_ = 0;
    std::int64_t window_dropped_ = 0;
    std::int64_t window_shown_ = 0;
};
