#include "oma/audio/mix.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numbers>
#include <utility>

namespace oma::audio {

float ClipGain::at(std::int64_t sample) const noexcept {
    if (sample < -lead || sample >= length + tail) {
        return 0.0F;
    }
    float g = gain;
    if (!automation.empty()) {
        // O(log k) per sample: k is at most a few hundred keys.
        const auto next = std::ranges::upper_bound(automation, sample, {}, &GainPoint::sample);
        if (next == automation.begin()) {
            g = automation.front().gain;
        } else if (next == automation.end()) {
            g = automation.back().gain;
        } else {
            const GainPoint& a = *std::prev(next);
            float t =
                static_cast<float>(sample - a.sample) / static_cast<float>(next->sample - a.sample);
            if (a.interpolation == 2)
                t = t * t * (3.0F - (2.0F * t));
            g = a.interpolation == 0 ? a.gain : a.gain + ((next->gain - a.gain) * t);
        }
    }
    if (fade_in > 0 && sample < fade_in) {
        g *= static_cast<float>(std::max<std::int64_t>(sample, 0)) / static_cast<float>(fade_in);
    }
    const std::int64_t remaining = length - sample; // 1 at the last sample
    if (fade_out > 0 && remaining <= fade_out) {
        g *= static_cast<float>(std::max<std::int64_t>(remaining - 1, 0)) /
             static_cast<float>(fade_out);
    }
    // Equal power keeps the level of two unrelated sounds steady across the crossfade.
    constexpr double kQuarter = std::numbers::pi / 2.0;
    if (lead > 0 && sample < lead) {
        const double x = static_cast<double>(sample + lead) / static_cast<double>(2 * lead);
        g *= static_cast<float>(std::sin(x * kQuarter));
    }
    if (tail > 0 && sample >= length - tail) {
        const double x =
            static_cast<double>(sample - (length - tail)) / static_cast<double>(2 * tail);
        g *= static_cast<float>(std::cos(x * kQuarter));
    }
    return g;
}

void mix_planar(std::span<float> output, int output_channels, std::span<const float> input,
                int input_channels, std::int64_t stride, std::int64_t frames, const ClipGain& gain,
                std::int64_t first_clip_sample) noexcept {
    if (output_channels <= 0 || input_channels <= 0 || frames <= 0) {
        return;
    }
    const auto out_ch = static_cast<std::size_t>(output_channels);
    const auto count =
        std::min<std::size_t>(static_cast<std::size_t>(frames), output.size() / out_ch);
    for (std::size_t i = 0; i < count; ++i) {
        const float g = gain.at(first_clip_sample + static_cast<std::int64_t>(i));
        if (g == 0.0F) {
            continue;
        }
        for (std::size_t c = 0; c < out_ch; ++c) {
            const std::size_t plane = input_channels == 1 ? 0 : c;
            if (std::cmp_greater_equal(plane, input_channels)) {
                continue;
            }
            const std::size_t at = (plane * static_cast<std::size_t>(stride)) + i;
            if (at < input.size()) {
                output[(i * out_ch) + c] += input[at] * g;
            }
        }
    }
}

} // namespace oma::audio
