#include "playback_clock.hpp"

#include <algorithm>
#include <cmath>

TickTarget tick_target(std::optional<std::int64_t> audible_sample, std::int64_t ticks_per_sample,
                       std::int64_t ticks_per_frame, std::int64_t play_from_frame,
                       double elapsed_seconds, double frame_rate, int speed, std::int64_t last) {
    std::int64_t target = 0;
    if (audible_sample) {
        target = (*audible_sample * ticks_per_sample / ticks_per_frame) + 1;
    } else {
        // Elapsed seconds are a disposable derived value here, never a stored time (CLAUDE.md §6).
        target = play_from_frame +
                 static_cast<std::int64_t>(std::floor(elapsed_seconds * frame_rate * speed));
    }
    const bool at_end = speed > 0 ? target >= last : target <= 0;
    return {.frame = std::clamp<std::int64_t>(target, 0, last), .at_end = at_end};
}
