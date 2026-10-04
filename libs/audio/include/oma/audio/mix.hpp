#pragma once

#include <cstdint>
#include <span>

// Mixing primitives for timeline audio (CLAUDE.md §12): plain arithmetic on float samples, no
// allocation, usable from any thread. Positions and lengths are in samples, never seconds.

namespace oma::audio {

// A clip's gain over its length: a constant gain with linear fades at both ends, and
// crossfades with its neighbours when transitions join them.
struct ClipGain {
    float gain = 1.0F;
    std::int64_t length = 0;   // clip length in samples
    std::int64_t fade_in = 0;  // samples
    std::int64_t fade_out = 0; // samples
    // Transitions: the clip also plays `lead` samples before its first and `tail` after its
    // last, under equal-power ramps over [-lead, lead) and [length - tail, length + tail).
    std::int64_t lead = 0;
    std::int64_t tail = 0;

    // The gain at `sample` (0 = the clip's first sample). Zero outside [-lead, length + tail).
    [[nodiscard]] float at(std::int64_t sample) const noexcept;
};

// Adds `frames` samples of planar input into interleaved output, scaled by the clip's gain at
// each sample. `input` holds `input_channels` planes of at least `frames` samples each, plane c
// at input[c * stride ...]. Mono input feeds every output channel; extra input channels beyond
// the output's are dropped (no downmix matrix yet).
void mix_planar(std::span<float> output, int output_channels, std::span<const float> input,
                int input_channels, std::int64_t stride, std::int64_t frames, const ClipGain& gain,
                std::int64_t first_clip_sample) noexcept;

} // namespace oma::audio
