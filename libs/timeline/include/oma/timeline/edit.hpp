#pragma once

#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"
#include "oma/timeline/editor.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Editing operations, each a Command for Editor::execute. Times are timeline times and must
// lie on the sequence grid (Timeline::to_ticks); source times are exact media times.
//
// Ripple operations shift the clips that follow on the same track only. Magnetism (closing
// gaps on a primary storyline) is a UI policy built from these generic commands
// (implementation plan M5), not a mode of the model.

namespace oma::timeline::edit {

// What a new clip shows: `duration` of timeline time starting at media time `source_in`.
struct ClipSource {
    MediaId media;
    RationalTime source_in;
    RationalTime duration; // timeline time
    TimeMap time_map;
    VideoProperties video;
    AudioProperties audio;
};

// Inserts the track at `index` (default: on top of the tracks of its kind order, i.e. last).
[[nodiscard]] std::unique_ptr<Command> add_track(TrackId id, TrackKind kind, std::string name,
                                                 std::optional<std::size_t> index = std::nullopt);
// Removes a track and its clips.
[[nodiscard]] std::unique_ptr<Command> remove_track(TrackId id);
[[nodiscard]] std::unique_ptr<Command> set_track_flags(TrackId id, bool muted, bool hidden);

// Places a clip at `at`, replacing whatever the range covers on the track (clips are trimmed,
// split or removed).
[[nodiscard]] std::unique_ptr<Command> overwrite(TrackId track, ClipId id, RationalTime at,
                                                 ClipSource source);
// Places a clip at `at` and pushes everything from `at` onward later by its duration; a clip
// spanning `at` is split.
[[nodiscard]] std::unique_ptr<Command> insert(TrackId track, ClipId id, RationalTime at,
                                              ClipSource source);
// Places a clip right after the last clip of the track.
[[nodiscard]] std::unique_ptr<Command> append(TrackId track, ClipId id, ClipSource source);

// Removes a clip and leaves a gap (lift).
[[nodiscard]] std::unique_ptr<Command> remove_clip(ClipId id);
// Removes a clip and closes the gap.
[[nodiscard]] std::unique_ptr<Command> ripple_delete(ClipId id);
// Removes the timeline range [from, to) from a track, trimming or splitting clips, and closes
// the gap.
[[nodiscard]] std::unique_ptr<Command> ripple_delete_range(TrackId track, RationalTime from,
                                                           RationalTime to);
// Splits a clip at a time strictly inside it; the right part gets a new ID.
[[nodiscard]] std::unique_ptr<Command> split(ClipId id, RationalTime at);

// Moves the clip's first instant to `new_start`, keeping its last instant (source in moves
// with it). With ripple, the clip stays in place and the clips after it move instead.
[[nodiscard]] std::unique_ptr<Command> trim_start(ClipId id, RationalTime new_start, bool ripple);
// Moves the clip's end to `new_end`. With ripple, the clips after it follow.
[[nodiscard]] std::unique_ptr<Command> trim_end(ClipId id, RationalTime new_end, bool ripple);
// Moves the edit point between a clip and the clip that starts exactly at its end.
[[nodiscard]] std::unique_ptr<Command> roll(ClipId left, RationalTime edit_point);
// Shows different media through the same timeline range: source in moves by `delta` of
// timeline time (scaled by the clip's speed).
[[nodiscard]] std::unique_ptr<Command> slip(ClipId id, RationalTime delta);
// Moves a clip by `delta` between its neighbours: an adjacent previous clip's end and an
// adjacent next clip's start follow; gaps absorb the move otherwise.
[[nodiscard]] std::unique_ptr<Command> slide(ClipId id, RationalTime delta);
// Moves a clip to another time and/or track (same kind); the destination must be free.
[[nodiscard]] std::unique_ptr<Command> move_clip(ClipId id, TrackId track, RationalTime start);

// Changes a constant speed keeping the same source range; the duration becomes
// source length / speed, rounded down to the sequence grid. With ripple, later clips follow.
[[nodiscard]] std::unique_ptr<Command> set_speed(ClipId id, Rational speed, bool ripple);
[[nodiscard]] std::unique_ptr<Command> set_video(ClipId id, VideoProperties video);
[[nodiscard]] std::unique_ptr<Command> set_audio(ClipId id, AudioProperties audio);
// Detaches a video clip's sound into audio clip `audio` on the audio track `lane`, at the same
// time and source range with the same audio adjustments, so the two can be trimmed apart (J- and
// L-cuts). The video clip then plays no sound. The range on the lane must be free.
[[nodiscard]] std::unique_ptr<Command> detach_audio(ClipId video, TrackId lane, ClipId audio);

[[nodiscard]] std::unique_ptr<Command> add_marker(MarkerId id, RationalTime at, std::string name);
[[nodiscard]] std::unique_ptr<Command> remove_marker(MarkerId id);

// Several commands as one history entry, applied in order; each sees the state left by the
// previous ones. All or nothing.
[[nodiscard]] std::unique_ptr<Command> transaction(std::string name,
                                                   std::vector<std::unique_ptr<Command>> commands);

} // namespace oma::timeline::edit
