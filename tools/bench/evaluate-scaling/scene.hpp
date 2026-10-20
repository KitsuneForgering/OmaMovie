#pragma once

// Synthetic scene for the evaluate-scaling benchmark: TRACKS video tracks of back-to-back
// one-second clips that cycle through MEDIA entries.
#include "oma/timeline/edit.hpp"
#include "oma/timeline/editor.hpp"

#include <cstdint>

namespace evaluate_scaling {

using namespace oma::timeline;

constexpr std::int64_t kClipFrames = 30;

inline oma::RationalTime f(std::int64_t frames) {
    return oma::RationalTime::make(frames, oma::Rational::make(1, 30).value()).value();
}

inline bool build(Editor& ed, std::uint64_t tracks, std::uint64_t media, std::uint64_t clips) {
    for (std::uint64_t i = 1; i <= media; ++i) {
        const MediaInfo m{.id = MediaId{i},
                          .start = f(0),
                          .duration = f(100000),
                          .has_video = true,
                          .has_audio = true,
                          .still = false};
        if (!ed.add_media(m)) {
            return false;
        }
    }
    for (std::uint64_t t = 0; t < tracks; ++t) {
        const TrackId track = ed.new_track_id();
        if (!ed.execute(edit::add_track(track, TrackKind::Video, "V"))) {
            return false;
        }
        for (std::uint64_t c = 0; c < clips; ++c) {
            // Highest IDs first, so the linear media lookup reaches the end of the vector.
            const edit::ClipSource src{.media = MediaId{media - ((t * clips + c) % media)},
                                       .source_in = f(0),
                                       .duration = f(kClipFrames),
                                       .time_map = {},
                                       .video = {},
                                       .audio = {}};
            const auto at = f(static_cast<std::int64_t>(c) * kClipFrames);
            if (!ed.execute(edit::overwrite(track, ed.new_clip_id(), at, src))) {
                return false;
            }
        }
    }
    return true;
}

} // namespace evaluate_scaling
