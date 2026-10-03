#pragma once

// Shared helpers for the timeline tests. The sequence runs at 30 fps with a 1/30 timebase, so
// one tick is one frame and layouts read in frames.

#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <format>
#include <string>

namespace timeline_test {

using namespace oma::timeline;

inline oma::Rational q(std::int64_t num, std::int64_t den) {
    return oma::Rational::make(num, den).value();
}

// `frames` of sequence time (1/30 s each).
inline oma::RationalTime f(std::int64_t frames) {
    return oma::RationalTime::make(frames, q(1, 30)).value();
}

// Media time in a 90 kHz timebase, given in 30 fps frames (3000 ticks each).
inline oma::RationalTime mf(std::int64_t frames) {
    return oma::RationalTime::make(frames * 3000, q(1, 90000)).value();
}

inline const MediaId kCamera{1};  // video + audio, 300 frames
inline const MediaId kMusic{2};   // audio only, 60 s at 48 kHz
inline const MediaId kPicture{3}; // still image

struct Fixture {
    Editor editor;
    TrackId video;
    TrackId overlay;
    TrackId audio;
};

inline Fixture make_fixture() {
    Fixture fx{Editor(Timeline::create(oma::frame_rates::k30, q(1, 30)).value()), {}, {}, {}};
    Editor& ed = fx.editor;
    (void)ed.add_media(MediaInfo{.id = kCamera,
                                 .start = mf(0),
                                 .duration = mf(300),
                                 .has_video = true,
                                 .has_audio = true,
                                 .still = false});
    (void)ed.add_media(
        MediaInfo{.id = kMusic,
                  .start = oma::RationalTime::make(0, q(1, 48000)).value(),
                  .duration = oma::RationalTime::make(60 * 48000, q(1, 48000)).value(),
                  .has_video = false,
                  .has_audio = true,
                  .still = false});
    (void)ed.add_media(MediaInfo{.id = kPicture,
                                 .start = {},
                                 .duration = {},
                                 .has_video = true,
                                 .has_audio = false,
                                 .still = true});
    fx.video = ed.new_track_id();
    fx.overlay = ed.new_track_id();
    fx.audio = ed.new_track_id();
    (void)ed.execute(edit::add_track(fx.video, TrackKind::Video, "V1"));
    (void)ed.execute(edit::add_track(fx.overlay, TrackKind::Video, "V2"));
    (void)ed.execute(edit::add_track(fx.audio, TrackKind::Audio, "A1"));
    return fx;
}

// Camera media from frame `in`, `duration` frames long.
inline edit::ClipSource camera(std::int64_t in, std::int64_t duration) {
    return edit::ClipSource{.media = kCamera,
                            .source_in = mf(in),
                            .duration = f(duration),
                            .time_map = {},
                            .video = {},
                            .audio = {}};
}

// Places a camera clip with overwrite and returns its ID (invalid if the edit failed).
inline ClipId place(Fixture& fx, TrackId track, std::int64_t at, std::int64_t in,
                    std::int64_t duration) {
    const ClipId id = fx.editor.new_clip_id();
    return fx.editor.execute(edit::overwrite(track, id, f(at), camera(in, duration))) ? id
                                                                                      : ClipId{};
}

// "start+duration@source" per clip, in frames; source shown in 30 fps frames, '~' if inexact.
inline std::string layout(const Timeline& tl, TrackId track) {
    std::string out;
    for (const Clip& c : tl.find_track(track)->clips) {
        const auto src = c.source_in.rescaled(q(1, 30), oma::Rounding::Floor).value();
        const bool exact = src == c.source_in;
        out += std::format("{}{}+{}@{}{}", out.empty() ? "" : " ", c.start.value(),
                           c.duration.value(), src.value(), exact ? "" : "~");
    }
    return out;
}

inline std::string layout(const Fixture& fx) {
    return layout(fx.editor.timeline(), fx.video);
}

} // namespace timeline_test
