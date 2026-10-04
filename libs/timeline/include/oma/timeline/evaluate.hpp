#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <vector>

// What the timeline shows and plays at one instant (CLAUDE.md §9.1): plain data, testable
// without a GPU. The timeline does not depend on the compositor (§5.2), so this is described in
// timeline terms (media IDs and media times); the playback layer, which owns the decoded
// frames, turns it into the compositor's RenderGraph by resolving each media time to an input.

namespace oma::timeline {

struct VideoLayer {
    ClipId clip;
    MediaId media;
    // The media time to show: the clip's exact source time rounded down to the media grid
    // (the frame on screen at t is the last one with PTS <= t).
    RationalTime media_time;
    VideoProperties video;
};

struct AudioSource {
    ClipId clip;
    MediaId media;
    RationalTime media_time;  // first sample, rounded down to the media grid
    RationalTime clip_offset; // time since the clip start, for fades
    RationalTime clip_duration;
    Rational speed;
    AudioProperties audio;
};

struct Composition {
    RationalTime time;
    std::vector<VideoLayer> video; // bottom first
    std::vector<AudioSource> audio;
};

// Hidden video tracks contribute no layers, muted tracks and clips no audio. Clips on video
// tracks contribute their audio when their media has some and it was not detached. Media times are
// expressed in the media's own timebase (MediaInfo::start).
[[nodiscard]] Result<Composition> evaluate(const Timeline& timeline, const RationalTime& at);

} // namespace oma::timeline
