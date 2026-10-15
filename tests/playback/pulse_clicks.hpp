#pragma once

// The long-offset fixture placed on a sequence, and where its clicks land (test_long_offset.cpp).
#include "playback_test.hpp"

namespace playback_test {

inline const tl::MediaId kPulse{
    9}; // pulse_3h.mkv: clicks at samples 24000 and 60000 of 2 s, 48 kHz
inline constexpr std::int64_t kStartMs = 10'800'000;

// Sequence samples holding a click, rendered in blocks of `block` samples.
inline std::vector<std::int64_t> clicks(std::int64_t block) {
    Sequence seq = make_sequence();
    (void)seq.editor.add_media(
        tl::MediaInfo{.id = kPulse,
                      .start = oma::RationalTime::make(kStartMs, q(1, 1000)).value(),
                      .duration = oma::RationalTime::make(2000, q(1, 1000)).value(),
                      .has_video = false,
                      .has_audio = true,
                      .still = false});
    // From 23,000 samples after the media start, placed at sequence sample 1000.
    const auto in = oma::RationalTime::make(kStartMs * 48 + 23'000, q(1, 48000)).value();
    const tl::edit::ClipSource source{.media = kPulse,
                                      .source_in = in,
                                      .duration = s(40'000),
                                      .time_map = {},
                                      .video = {},
                                      .audio = {}};
    if (!seq.editor.execute(
            tl::edit::overwrite(seq.audio, seq.editor.new_clip_id(), s(1000), source)))
        return {};
    auto files = paths();
    files[kPulse.value()] = fixture("pulse_3h.mkv").string();
    oma::playback::TimelineAudio audio(seq.editor.timeline(), files, rate(), kChannels);
    std::vector<float> out(static_cast<std::size_t>(48'000 * kChannels));
    for (std::int64_t at = 0; at < 48'000; at += block) {
        const std::int64_t n = std::min(block, 48'000 - at);
        if (!audio.render(std::span<float>(out).subspan(static_cast<std::size_t>(at * kChannels),
                                                        static_cast<std::size_t>(n * kChannels)),
                          at))
            return {};
    }
    std::vector<std::int64_t> found;
    for (std::int64_t i = 0; i < 48'000; ++i)
        if (std::abs(out[static_cast<std::size_t>(i * kChannels)]) > 0.5F)
            found.push_back(i);
    return found;
}

} // namespace playback_test
