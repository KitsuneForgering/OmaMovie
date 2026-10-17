#include "audio_gaps.hpp"

#include <algorithm>

namespace oma::media::detail {

FramePlacement place_frame(std::optional<std::int64_t> stamped, std::int64_t at, int n,
                           int rate_hz) {
    if (!stamped) {
        return {};
    }
    const std::int64_t tolerance = static_cast<std::int64_t>(rate_hz) * 20 / 1000;
    const std::int64_t limit = static_cast<std::int64_t>(rate_hz) * 10;
    const std::int64_t drift = *stamped - at;
    if (drift > limit || drift < -limit) {
        return {.silence = 0, .drop = 0, .anchor = stamped};
    }
    if (drift > tolerance) {
        return {.silence = drift, .drop = 0, .anchor = std::nullopt};
    }
    if (drift < -tolerance) {
        return {.silence = 0, .drop = std::min<std::int64_t>(-drift, n), .anchor = std::nullopt};
    }
    return {};
}

AudioBuffer with_silence(const std::vector<float>& planar, int capacity, int channels,
                         std::int64_t gap, int first, int kept, SampleRate rate,
                         std::int64_t start) {
    const auto frames = static_cast<std::size_t>(gap) + static_cast<std::size_t>(kept);
    std::vector<float> samples(frames * static_cast<std::size_t>(channels), 0.0F);
    const auto run = static_cast<std::size_t>(kept);
    for (int c = 0; c < channels; ++c) {
        const auto plane = static_cast<std::size_t>(c);
        const auto src =
            (plane * static_cast<std::size_t>(capacity)) + static_cast<std::size_t>(first);
        const auto dst = (plane * frames) + static_cast<std::size_t>(gap);
        std::copy_n(planar.data() + src, run, samples.data() + dst);
    }
    return AudioBuffer{.pts = rate.sample_to_time(start),
                       .sample_rate = rate,
                       .channels = channels,
                       .frames = static_cast<std::int64_t>(frames),
                       .samples = std::move(samples)};
}

} // namespace oma::media::detail
