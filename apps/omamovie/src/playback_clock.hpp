#pragma once

// The per-tick decision of the transport (Session::onTick), as a pure function so it can be
// tested without a window, a device or real time.

#include <cstdint>
#include <optional>

struct TickTarget {
    std::int64_t frame = 0; // the frame to show, clamped to [0, last]
    bool at_end = false;    // the clock reached the end in its direction
};

// At 1x with audio running, video follows the sample actually heard, one frame ahead because the
// chosen frame still has to cross Qt's render loop (CLAUDE.md §12). Otherwise (other speeds, no
// audio) it follows the elapsed time at `speed` from `play_from_frame`.
[[nodiscard]] TickTarget tick_target(std::optional<std::int64_t> audible_sample,
                                     std::int64_t ticks_per_sample, std::int64_t ticks_per_frame,
                                     std::int64_t play_from_frame, double elapsed_seconds,
                                     double frame_rate, int speed, std::int64_t last);
