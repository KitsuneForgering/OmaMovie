#pragma once

// Helpers for the constant-speed sound tests: the tone fixture placed at a speed, and the
// frequency heard, measured from zero crossings.
#include "playback_test.hpp"

namespace playback_test {

// `samples` of the 440 Hz tone at constant `speed`, from media sample `in` (44.1 kHz).
inline tl::ClipId place_at_speed(Sequence& seq, std::int64_t at, std::int64_t in,
                                 std::int64_t samples, oma::Rational speed) {
    const tl::ClipId id = seq.editor.new_clip_id();
    const tl::edit::ClipSource source{.media = kTone,
                                      .source_in = oma::RationalTime::make(in, q(1, 44100)).value(),
                                      .duration = s(samples),
                                      .time_map = tl::TimeMap::constant(speed).value(),
                                      .video = {},
                                      .audio = {}};
    return seq.editor.execute(tl::edit::overwrite(seq.audio, id, s(at), source)) ? id
                                                                                 : tl::ClipId{};
}

// Frequency of the first channel over interleaved frames [from, to), from rising zero crossings.
inline double frequency(const std::vector<float>& out, std::int64_t from, std::int64_t to) {
    int rising = 0;
    for (std::int64_t i = from + 1; i < to; ++i)
        rising += out[static_cast<std::size_t>((i - 1) * kChannels)] < 0.0F &&
                  out[static_cast<std::size_t>(i * kChannels)] >= 0.0F;
    return rising * 48000.0 / static_cast<double>(to - from);
}

} // namespace playback_test
