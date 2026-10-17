#pragma once

// Shared helpers for the playback tests: a 48 kHz sequence (one tick per sample at 30 fps)
// over the generated audio fixtures.

#include "oma/playback/timeline_audio.hpp"
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"
#include "oma/timeline/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace playback_test {

namespace tl = oma::timeline;

inline constexpr int kChannels = 2;
inline const tl::MediaId kTone{1};   // tone_44100.wav: 2 s of 440 Hz at amplitude 1/8
inline const tl::MediaId kCamera{2}; // h264_30fps_aac.mp4: 1 s of video with a 440 Hz AAC tone
inline const tl::MediaId kNoisy{3};  // noisy_tone.wav: 3 s of hiss, the tone from 1 s on (48 kHz)

inline std::filesystem::path fixture(const char* name) {
    const char* dir = std::getenv("OMA_FIXTURES");
    return std::filesystem::path(dir != nullptr ? dir : "tests/fixtures/generated") / name;
}

inline bool have_fixture(const char* name) {
    if (std::filesystem::exists(fixture(name))) {
        return true;
    }
    std::printf("    (skipped: fixture %s was not generated)\n", name);
    return false;
}

inline oma::Rational q(std::int64_t num, std::int64_t den) {
    return oma::Rational::make(num, den).value();
}

inline oma::SampleRate rate() {
    return oma::SampleRate::make(48000).value();
}

// Sequence time in samples (the sequence timebase is 1/48000).
inline oma::RationalTime s(std::int64_t samples) {
    return oma::RationalTime::make(samples, q(1, 48000)).value();
}

struct Sequence {
    tl::Editor editor;
    tl::TrackId audio;
    tl::TrackId video;
};

inline Sequence make_sequence() {
    Sequence seq{tl::Editor(tl::Timeline::create(oma::frame_rates::k30, rate()).value()), {}, {}};
    (void)seq.editor.add_media(
        tl::MediaInfo{.id = kTone,
                      .start = oma::RationalTime::make(0, q(1, 44100)).value(),
                      .duration = oma::RationalTime::make(88200, q(1, 44100)).value(),
                      .has_video = false,
                      .has_audio = true,
                      .still = false});
    (void)seq.editor.add_media(
        tl::MediaInfo{.id = kCamera,
                      .start = oma::RationalTime::make(0, q(1, 15360)).value(),
                      .duration = oma::RationalTime::make(15360, q(1, 15360)).value(),
                      .has_video = true,
                      .has_audio = true,
                      .still = false});
    (void)seq.editor.add_media(
        tl::MediaInfo{.id = kNoisy,
                      .start = oma::RationalTime::make(0, q(1, 48000)).value(),
                      .duration = oma::RationalTime::make(144000, q(1, 48000)).value(),
                      .has_video = false,
                      .has_audio = true,
                      .still = false});
    seq.video = seq.editor.new_track_id();
    seq.audio = seq.editor.new_track_id();
    (void)seq.editor.execute(tl::edit::add_track(seq.video, tl::TrackKind::Video, "V1"));
    (void)seq.editor.execute(tl::edit::add_track(seq.audio, tl::TrackKind::Audio, "A1"));
    return seq;
}

// Places `samples` of the tone from media sample `in` (44.1 kHz) at sequence sample `at`.
inline tl::ClipId place(Sequence& seq, std::int64_t at, std::int64_t in, std::int64_t samples,
                        tl::AudioProperties audio = {}) {
    const tl::ClipId id = seq.editor.new_clip_id();
    const tl::edit::ClipSource source{.media = kTone,
                                      .source_in = oma::RationalTime::make(in, q(1, 44100)).value(),
                                      .duration = s(samples),
                                      .time_map = {},
                                      .video = {},
                                      .audio = audio};
    return seq.editor.execute(tl::edit::overwrite(seq.audio, id, s(at), source)) ? id
                                                                                 : tl::ClipId{};
}

// The whole camera fixture on the video track at sequence sample `at`.
inline tl::ClipId place_camera(Sequence& seq, std::int64_t at) {
    const tl::ClipId id = seq.editor.new_clip_id();
    const tl::edit::ClipSource source{.media = kCamera,
                                      .source_in = oma::RationalTime::make(0, q(1, 15360)).value(),
                                      .duration = s(48000),
                                      .time_map = {},
                                      .video = {},
                                      .audio = {}};
    return seq.editor.execute(tl::edit::overwrite(seq.video, id, s(at), source)) ? id
                                                                                 : tl::ClipId{};
}

inline std::unordered_map<std::uint64_t, std::string> paths() {
    return {{kTone.value(), fixture("tone_44100.wav").string()},
            {kCamera.value(), fixture("h264_30fps_aac.mp4").string()},
            {kNoisy.value(), fixture("noisy_tone.wav").string()}};
}

// Renders `frames` sequence samples from `first`, in blocks of `block` frames.
inline std::vector<float> render(const tl::Timeline& timeline, std::int64_t first,
                                 std::int64_t frames, std::int64_t block = 1024) {
    oma::playback::TimelineAudio audio(timeline, paths(), rate(), kChannels);
    std::vector<float> out(static_cast<std::size_t>(frames * kChannels));
    for (std::int64_t at = 0; at < frames; at += block) {
        const std::int64_t n = std::min(block, frames - at);
        const auto span = std::span<float>(out).subspan(static_cast<std::size_t>(at * kChannels),
                                                        static_cast<std::size_t>(n * kChannels));
        if (!audio.render(span, first + at)) {
            return {};
        }
    }
    return out;
}

// The whole noisy fixture on the audio track at 0.
inline tl::ClipId place_noisy(Sequence& seq, tl::AudioProperties audio = {}) {
    const tl::ClipId id = seq.editor.new_clip_id();
    const tl::edit::ClipSource source{.media = kNoisy,
                                      .source_in = oma::RationalTime::make(0, q(1, 48000)).value(),
                                      .duration = s(144000),
                                      .time_map = {},
                                      .video = {},
                                      .audio = audio};
    return seq.editor.execute(tl::edit::overwrite(seq.audio, id, s(0), source)) ? id : tl::ClipId{};
}

// RMS level in dBFS of interleaved frames [from, to).
inline double level_db(const std::vector<float>& samples, std::int64_t from, std::int64_t to) {
    double squares = 0.0;
    for (std::int64_t i = from * kChannels; i < to * kChannels; ++i) {
        const double v = samples[static_cast<std::size_t>(i)];
        squares += v * v;
    }
    const auto n = static_cast<double>((to - from) * kChannels);
    return squares > 0.0 ? 10.0 * std::log10(squares / n) : -200.0;
}

// Largest absolute sample in interleaved frames [from, to).
inline float peak(const std::vector<float>& samples, std::int64_t from, std::int64_t to) {
    float p = 0.0F;
    for (std::int64_t i = from * kChannels; i < to * kChannels; ++i) {
        p = std::max(p, std::abs(samples[static_cast<std::size_t>(i)]));
    }
    return p;
}

} // namespace playback_test
