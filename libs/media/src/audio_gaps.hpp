#pragma once

#include "oma/media/audio_decoder.hpp"

#include <cstdint>
#include <optional>
#include <vector>

// Where a decoded audio frame plays when its timestamp disagrees with the samples delivered so
// far (AudioDecoder::Impl::convert). Pure arithmetic, separate so it is easy to test.
namespace oma::media::detail {

struct FramePlacement {
    std::int64_t silence = 0;           // samples of silence before the frame (a gap)
    std::int64_t drop = 0;              // samples to drop from the frame's start (an overlap)
    std::optional<std::int64_t> anchor; // a jump too large to fill: continue from here
};

// `stamped`: the frame's first sample by its timestamp (none: trust the count); `at`: the next
// sample by count; `n`: the frame's samples. Differences within 20 ms are left alone (resampler
// delay, rounding); gaps beyond 10 s re-anchor instead of allocating that much silence.
[[nodiscard]] FramePlacement place_frame(std::optional<std::int64_t> stamped, std::int64_t at,
                                         int n, int rate_hz);

// The frame's `kept` samples per channel (planar, laid out `capacity` apart, from `first`)
// after `gap` samples of silence, as one buffer starting at `start`.
[[nodiscard]] AudioBuffer with_silence(const std::vector<float>& planar, int capacity, int channels,
                                       std::int64_t gap, int first, int kept, SampleRate rate,
                                       std::int64_t start);

} // namespace oma::media::detail
