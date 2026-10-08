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
    // A title instead of media (ADR-0015): `media` stays 0.
    std::optional<Title> title = std::nullopt;
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
// timeline time (scaled by the clip's constant speed; at speed 1 for a segmented map).
[[nodiscard]] std::unique_ptr<Command> slip(ClipId id, RationalTime delta);
// Moves a clip by `delta` between its neighbours: an adjacent previous clip's end and an
// adjacent next clip's start follow; gaps absorb the move otherwise.
[[nodiscard]] std::unique_ptr<Command> slide(ClipId id, RationalTime delta);
// Moves a clip to another time and/or track (same kind); the destination must be free.
[[nodiscard]] std::unique_ptr<Command> move_clip(ClipId id, TrackId track, RationalTime start);
// Moves a clip along its own track, closing the gap it leaves and pushing later clips to make
// room where it lands (a storyline reorder). `at` is measured with the clip already taken out
// and must not fall inside another clip. Transitions into the moved clip, into the clip that
// followed it and into the clip it now precedes are removed: their clip pairs no longer meet.
[[nodiscard]] std::unique_ptr<Command> ripple_move(ClipId id, RationalTime at);

// Changes a constant speed keeping the same source range; the duration becomes
// source length / speed, rounded down to the sequence grid. With ripple, later clips follow.
[[nodiscard]] std::unique_ptr<Command> set_speed(ClipId id, Rational speed, bool ripple);
// Connects `dependent` to `primary` on another track (ADR-0014): from now on it follows the
// primary's content. Its start must lie on the primary, which plays at a constant speed and is
// not connected itself.
[[nodiscard]] std::unique_ptr<Command> connect(ClipId dependent, ClipId primary);
// Ends a connection; the clip stays where it is.
[[nodiscard]] std::unique_ptr<Command> disconnect(ClipId dependent);
// Replaces a clip's time map (ADR-0013); a segmented map sets the duration to its length.
// source_in stays the media position at the clip's first instant.
[[nodiscard]] std::unique_ptr<Command> set_time_map(ClipId id, TimeMap map, bool ripple);
// Plays the clip backwards over the same media and duration.
[[nodiscard]] std::unique_ptr<Command> reverse(ClipId id);
// Holds the frame at `at` (sequence time inside the clip) for `length`, then continues; the
// clip grows by `length`. With ripple, later clips follow.
[[nodiscard]] std::unique_ptr<Command> freeze_frame(ClipId id, RationalTime at, RationalTime length,
                                                    bool ripple);
[[nodiscard]] std::unique_ptr<Command> set_video(ClipId id, VideoProperties video);
[[nodiscard]] std::unique_ptr<Command> set_audio(ClipId id, AudioProperties audio);
// Replaces a title clip's text and style (ADR-0015); fails on a media clip.
[[nodiscard]] std::unique_ptr<Command> set_title(ClipId id, Title title);
// Sets or removes (std::nullopt) the transition into a clip from the clip before it.
[[nodiscard]] std::unique_ptr<Command> set_transition(ClipId id,
                                                      std::optional<Transition> transition);
// Detaches a video clip's sound into audio clip `audio` on the audio track `lane`, at the same
// time and source range with the same audio adjustments, so the two can be trimmed apart (J- and
// L-cuts). The video clip then plays no sound. The range on the lane must be free.
[[nodiscard]] std::unique_ptr<Command> detach_audio(ClipId video, TrackId lane, ClipId audio);

[[nodiscard]] std::unique_ptr<Command> add_marker(MarkerId id, RationalTime at, std::string name);
[[nodiscard]] std::unique_ptr<Command> remove_marker(MarkerId id);
// Captions (ADR-0017). Times are sequence times on the grid; overlapping or off-grid captions
// are refused by validation, never moved or rounded.
[[nodiscard]] std::unique_ptr<Command> add_caption(Caption caption);
[[nodiscard]] std::unique_ptr<Command> set_caption(Caption caption); // the caption with its ID
[[nodiscard]] std::unique_ptr<Command> remove_caption(CaptionId id);
// The canvas size (ADR-0010): even, 16 to 16384 pixels each way. Transform offsets (and their
// keys) are pixels of the canvas, so they scale by the same ratio on each axis and every layer
// keeps its place relative to the frame. From an unset canvas (0) nothing is rescaled.
[[nodiscard]] std::unique_ptr<Command> set_canvas(std::uint32_t width, std::uint32_t height);
// The whole list at once (an import), one undo entry.
[[nodiscard]] std::unique_ptr<Command> replace_captions(std::vector<Caption> captions);

// Several commands as one history entry, applied in order; each sees the state left by the
// previous ones. All or nothing.
[[nodiscard]] std::unique_ptr<Command> transaction(std::string name,
                                                   std::vector<std::unique_ptr<Command>> commands);

} // namespace oma::timeline::edit
