#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"

#include <cstdint>

// Playback master clock (CLAUDE.md §12): the position that is audible now, derived from the
// frames the device actually consumed, never from wall time. Video follows it by dropping or
// repeating frames.
//
// There is no separate monotonic fallback: the timeline mix always produces samples (silence
// where no clip plays), and machines without an audio device consume them through the null
// output, which is paced by a steady clock.

namespace oma::audio {

class PlaybackClock {
public:
    explicit PlaybackClock(SampleRate rate) noexcept : rate_(rate) {}

    [[nodiscard]] SampleRate rate() const noexcept { return rate_; }

    // Ties a media position to the device's consumed count, after a seek (output flushed) or
    // before the first write. The position is rounded to the nearest sample.
    [[nodiscard]] Result<void> anchor(const RationalTime& position, std::int64_t frames_consumed);

    // The audible position: the anchor plus the frames consumed since then, minus the frames
    // still buffered in the device. It never goes before the anchor (latency larger than what
    // was consumed means nothing from this anchor is audible yet). Expressed in samples.
    [[nodiscard]] RationalTime position(std::int64_t frames_consumed,
                                        std::int64_t latency_frames) const noexcept;

    // The anchor in samples (the first sample written after it).
    [[nodiscard]] std::int64_t anchor_sample() const noexcept { return anchor_sample_; }

private:
    SampleRate rate_;
    std::int64_t anchor_sample_ = 0;
    std::int64_t anchor_consumed_ = 0;
};

} // namespace oma::audio
