#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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
    // The clip's properties at this instant: keyframed values are evaluated into their plain
    // fields (transform) and the keys are left out.
    VideoProperties video;
    // Set by a transition: a factor on the clip's opacity, and the fraction of the output width
    // the layer shows, from the left (a wipe). 1 and 1 outside transitions.
    float opacity = 1.0F;
    double reveal = 1.0;
    // A title clip (ADR-0015): no media; the app draws this instead of decoding a picture.
    std::optional<Title> title = std::nullopt;
};

struct AudioSource {
    ClipId clip;
    MediaId media;
    RationalTime media_time;  // first sample, rounded down to the media grid
    RationalTime clip_offset; // time since the clip start, for fades
    RationalTime clip_duration;
    Rational speed; // the constant speed; 0 for a segmented map (freeze, reverse, ramps)
    AudioProperties audio;
};

struct Composition {
    RationalTime time;
    std::vector<VideoLayer> video; // bottom first
    std::vector<AudioSource> audio;
};

// Where a clip's incoming transition plays: [cut - half, cut + half) in sequence ticks, cut being
// the clip's start. `half` is the transition's half length, shortened to the media both clips
// have to spare past the cut and to the clips' own lengths.
struct TransitionWindow {
    const Clip* from = nullptr; // the clip ending at the cut
    const Clip* to = nullptr;   // the clip with the transition
    TransitionKind kind = TransitionKind::Dissolve;
    std::int64_t cut = 0;
    std::int64_t half = 0;

    // Position in the transition, in [0, 1), for ticks inside the window.
    [[nodiscard]] double progress(std::int64_t ticks) const noexcept;
};

// The window of the transition into track.clips[index], or std::nullopt when it has none, no
// clip ends where it starts, or there is no media to spare.
[[nodiscard]] std::optional<TransitionWindow>
transition_window(const Timeline& timeline, const Track& track, std::size_t index);

// The exact source time (Clip::source_in's terms, not rounded to the media) at sequence `ticks`
// of clip `c`; ticks outside the clip extrapolate at its speed.
[[nodiscard]] Result<RationalTime> source_time(const Timeline& timeline, const Clip& c,
                                               std::int64_t ticks);

// The transform of `video` at exact source time `source`: its transform without keys, the first
// or last key's value outside them, else the interpolation from the key before. Scale moves
// geometrically (a steady zoom) when both keys have the same sign, linearly otherwise.
[[nodiscard]] Transform transform_at(const VideoProperties& video, const RationalTime& source);
// A keyed number at exact source time `source` (`keys` not empty): before the first key its
// value, after the last its value, else the interpolation of the key before it.
[[nodiscard]] double scalar_at(std::span<const ScalarKey> keys, const RationalTime& source);

// Hidden video tracks contribute no layers, muted tracks and clips no audio. Inside a
// transition a video track contributes both clips, outgoing first, with their transition
// opacity and reveal; audio sources list each clip on its own (playback mixes transitions). Clips
// on video tracks contribute their audio when their media has some and it was not detached. Media
// times are expressed in the media's own timebase (MediaInfo::start).
[[nodiscard]] Result<Composition> evaluate(const Timeline& timeline, const RationalTime& at);

} // namespace oma::timeline
