#include "oma/audio/clock.hpp"

#include <algorithm>

namespace oma::audio {

Result<void> PlaybackClock::anchor(const RationalTime& position, std::int64_t frames_consumed) {
    auto sample = rate_.time_to_sample(position, Rounding::Nearest);
    if (!sample) {
        return std::unexpected(sample.error());
    }
    anchor_sample_ = *sample;
    anchor_consumed_ = frames_consumed;
    return {};
}

RationalTime PlaybackClock::position(std::int64_t frames_consumed,
                                     std::int64_t latency_frames) const noexcept {
    const std::int64_t audible =
        std::max<std::int64_t>(frames_consumed - anchor_consumed_ - latency_frames, 0);
    return rate_.sample_to_time(anchor_sample_ + audible);
}

} // namespace oma::audio
