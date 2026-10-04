#include "oma/timeline/edit.hpp"

#include "commands.hpp"
#include "mutation.hpp"

#include <algorithm>
#include <utility>

namespace oma::timeline::edit {

namespace {

using detail::error;
using detail::Steps;

Result<const Clip*> get_clip(const Timeline& tl, ClipId id) {
    const Clip* c = tl.find_clip(id);
    if (c == nullptr) {
        return error(ErrorCode::InvalidArgument, "unknown clip", detail::clip_context(id));
    }
    return c;
}

Result<const Track*> get_track(const Timeline& tl, TrackId id) {
    const Track* t = tl.find_track(id);
    if (t == nullptr) {
        return error(ErrorCode::InvalidArgument, "unknown track", detail::track_context(id));
    }
    return t;
}

// The clip with its first instant moved to `new_start` and its end kept: the source in moves
// by the same amount of timeline time, mapped exactly through the clip's speed.
Result<Clip> with_start(const Timeline& tl, Clip c, std::int64_t new_start) {
    const std::int64_t end = c.end_ticks();
    if (new_start >= end || new_start < 0) {
        return error(ErrorCode::InvalidArgument, "the clip would be empty",
                     detail::clip_context(c.id));
    }
    auto offset = c.time_map.media_offset(new_start - c.start.value(), tl.timebase());
    if (!offset) {
        return std::unexpected(offset.error());
    }
    auto source_in = detail::add_exact(c.source_in, *offset);
    if (!source_in) {
        return std::unexpected(source_in.error());
    }
    c.source_in = *source_in;
    c.start = tl.at(new_start);
    c.duration = tl.at(end - new_start);
    return c;
}

Result<Clip> with_end(const Timeline& tl, Clip c, std::int64_t new_end) {
    if (new_end <= c.start.value()) {
        return error(ErrorCode::InvalidArgument, "the clip would be empty",
                     detail::clip_context(c.id));
    }
    c.duration = tl.at(new_end - c.start.value());
    return c;
}

std::int64_t track_end(const Track& t) {
    return t.clips.empty() ? 0 : t.clips.back().end_ticks();
}

// Empties [from, to) on a track: clips inside are removed, clips crossing an edge are trimmed,
// and a clip covering the whole range is split around it (the tail gets a new ID).
Result<Steps> clear_range(Timeline& tl, const Track& track, std::int64_t from, std::int64_t to) {
    Steps steps;
    for (const Clip& c : track.clips) {
        const std::int64_t start = c.start_ticks();
        const std::int64_t end = c.end_ticks();
        if (end <= from || start >= to) {
            continue;
        }
        if (start >= from && end <= to) {
            steps.push_back(detail::erase_clip(c.id));
        } else if (start < from && end > to) {
            auto head = with_end(tl, c, from);
            auto tail = with_start(tl, c, to);
            if (!head || !tail) {
                return std::unexpected(!head ? head.error() : tail.error());
            }
            tail->id = ClipId(detail::Mutation(tl).allocate_id());
            steps.push_back(detail::replace_clip(*head));
            steps.push_back(detail::insert_clip(track.id, *tail));
        } else if (start < from) {
            auto head = with_end(tl, c, from);
            if (!head) {
                return std::unexpected(head.error());
            }
            steps.push_back(detail::replace_clip(*head));
        } else {
            auto tail = with_start(tl, c, to);
            if (!tail) {
                return std::unexpected(tail.error());
            }
            steps.push_back(detail::replace_clip(*tail));
        }
    }
    return steps;
}

Result<Clip> new_clip(const Timeline& tl, ClipId id, std::int64_t start, const ClipSource& source) {
    auto duration = tl.to_ticks(source.duration);
    if (!duration) {
        return std::unexpected(duration.error());
    }
    if (*duration <= 0) {
        return error(ErrorCode::InvalidArgument, "clip duration must be positive",
                     detail::clip_context(id));
    }
    if (auto end = detail::add_ticks(start, *duration); !end) {
        return std::unexpected(end.error());
    }
    return Clip{.id = id,
                .media = source.media,
                .start = tl.at(start),
                .duration = tl.at(*duration),
                .source_in = source.source_in,
                .time_map = source.time_map,
                .video = source.video,
                .audio = source.audio};
}

// A change to a clip plus a shift of the clips after it. Whichever frees space runs first, so
// no intermediate state overlaps: growing shifts first, shrinking changes first.
Steps change_and_shift(Clip changed, TrackId track, std::int64_t shift_from,
                       std::int64_t shift_by) {
    Steps steps;
    auto change = detail::replace_clip(changed);
    auto shift = detail::shift_clips(track, shift_from, shift_by);
    if (shift_by > 0) {
        steps.push_back(std::move(shift));
        steps.push_back(std::move(change));
    } else {
        steps.push_back(std::move(change));
        steps.push_back(std::move(shift));
    }
    return steps;
}

std::unique_ptr<Command> single(std::string name, std::unique_ptr<Command> step) {
    Steps steps;
    steps.push_back(std::move(step));
    return detail::make_transaction(std::move(name), std::move(steps));
}

} // namespace

std::unique_ptr<Command> add_track(TrackId id, TrackKind kind, std::string name,
                                   std::optional<std::size_t> index) {
    return detail::make_planned(
        "Add Track", [=, name = std::move(name)](Timeline& tl) -> Result<Steps> {
            Steps steps;
            steps.push_back(detail::insert_track(Track{.id = id,
                                                       .kind = kind,
                                                       .name = name,
                                                       .muted = false,
                                                       .hidden = false,
                                                       .clips = {}},
                                                 index.value_or(tl.tracks().size())));
            return steps;
        });
}

std::unique_ptr<Command> remove_track(TrackId id) {
    return single("Remove Track", detail::erase_track(id));
}

std::unique_ptr<Command> set_track_flags(TrackId id, bool muted, bool hidden) {
    return single("Change Track", detail::set_flags(id, muted, hidden));
}

std::unique_ptr<Command> overwrite(TrackId track, ClipId id, RationalTime at, ClipSource source) {
    return detail::make_planned("Overwrite", [=](Timeline& tl) -> Result<Steps> {
        auto t = get_track(tl, track);
        auto start = tl.to_ticks(at);
        if (!t || !start) {
            return std::unexpected(!t ? t.error() : start.error());
        }
        auto clip = new_clip(tl, id, *start, source);
        if (!clip) {
            return std::unexpected(clip.error());
        }
        auto steps = clear_range(tl, **t, *start, clip->end_ticks());
        if (!steps) {
            return steps;
        }
        steps->push_back(detail::insert_clip(track, *clip));
        return steps;
    });
}

std::unique_ptr<Command> insert(TrackId track, ClipId id, RationalTime at, ClipSource source) {
    return detail::make_planned("Insert", [=](Timeline& tl) -> Result<Steps> {
        auto t = get_track(tl, track);
        auto start = tl.to_ticks(at);
        if (!t || !start) {
            return std::unexpected(!t ? t.error() : start.error());
        }
        auto clip = new_clip(tl, id, *start, source);
        if (!clip) {
            return std::unexpected(clip.error());
        }
        const std::int64_t length = clip->duration.value();
        Steps steps;
        const Clip* spanning = tl.clip_at(track, *start);
        std::optional<Clip> tail;
        if (spanning != nullptr && spanning->start_ticks() < *start) {
            // The head ends at the insertion point; the tail keeps its source range and moves
            // right by the inserted length.
            auto head = with_end(tl, *spanning, *start);
            auto moved = with_start(tl, *spanning, *start);
            if (!head || !moved) {
                return std::unexpected(!head ? head.error() : moved.error());
            }
            moved->start = tl.at(*start + length);
            moved->id = ClipId(detail::Mutation(tl).allocate_id());
            tail = *moved;
            steps.push_back(detail::replace_clip(*head));
        }
        steps.push_back(detail::shift_clips(track, *start, length));
        if (tail) {
            steps.push_back(detail::insert_clip(track, *tail));
        }
        steps.push_back(detail::insert_clip(track, *clip));
        return steps;
    });
}

std::unique_ptr<Command> append(TrackId track, ClipId id, ClipSource source) {
    return detail::make_planned("Append", [=](Timeline& tl) -> Result<Steps> {
        auto t = get_track(tl, track);
        if (!t) {
            return std::unexpected(t.error());
        }
        auto clip = new_clip(tl, id, track_end(**t), source);
        if (!clip) {
            return std::unexpected(clip.error());
        }
        Steps steps;
        steps.push_back(detail::insert_clip(track, *clip));
        return steps;
    });
}

std::unique_ptr<Command> remove_clip(ClipId id) {
    return single("Delete", detail::erase_clip(id));
}

std::unique_ptr<Command> ripple_delete(ClipId id) {
    return detail::make_planned("Ripple Delete", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        if (!c) {
            return std::unexpected(c.error());
        }
        Steps steps;
        steps.push_back(detail::erase_clip(id));
        steps.push_back(
            detail::shift_clips(tl.track_of(id)->id, (*c)->end_ticks(), -(*c)->duration.value()));
        return steps;
    });
}

std::unique_ptr<Command> ripple_delete_range(TrackId track, RationalTime from, RationalTime to) {
    return detail::make_planned("Ripple Delete", [=](Timeline& tl) -> Result<Steps> {
        auto t = get_track(tl, track);
        auto a = tl.to_ticks(from);
        auto b = tl.to_ticks(to);
        if (!t || !a || !b) {
            return std::unexpected(!t ? t.error() : (!a ? a.error() : b.error()));
        }
        if (*b <= *a || *a < 0) {
            return error(ErrorCode::InvalidArgument, "empty or negative range",
                         detail::track_context(track));
        }
        auto steps = clear_range(tl, **t, *a, *b);
        if (!steps) {
            return steps;
        }
        steps->push_back(detail::shift_clips(track, *b, *a - *b));
        return steps;
    });
}

std::unique_ptr<Command> split(ClipId id, RationalTime at) {
    return detail::make_planned("Split", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto point = tl.to_ticks(at);
        if (!c || !point) {
            return std::unexpected(!c ? c.error() : point.error());
        }
        if (*point <= (*c)->start_ticks() || *point >= (*c)->end_ticks()) {
            return error(ErrorCode::InvalidArgument, "split point outside the clip",
                         detail::clip_context(id));
        }
        auto head = with_end(tl, **c, *point);
        auto tail = with_start(tl, **c, *point);
        if (!head || !tail) {
            return std::unexpected(!head ? head.error() : tail.error());
        }
        tail->id = ClipId(detail::Mutation(tl).allocate_id());
        Steps steps;
        steps.push_back(detail::replace_clip(*head));
        steps.push_back(detail::insert_clip(tl.track_of(id)->id, *tail));
        return steps;
    });
}

std::unique_ptr<Command> trim_start(ClipId id, RationalTime new_start, bool ripple) {
    return detail::make_planned("Trim", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto start = tl.to_ticks(new_start);
        if (!c || !start) {
            return std::unexpected(!c ? c.error() : start.error());
        }
        auto trimmed = with_start(tl, **c, *start);
        if (!trimmed) {
            return std::unexpected(trimmed.error());
        }
        if (!ripple) {
            Steps steps;
            steps.push_back(detail::replace_clip(*trimmed));
            return steps;
        }
        // Ripple: the clip keeps its position; what follows moves by the length change.
        const std::int64_t delta = *start - (*c)->start_ticks();
        trimmed->start = (*c)->start;
        return change_and_shift(*trimmed, tl.track_of(id)->id, (*c)->end_ticks(), -delta);
    });
}

std::unique_ptr<Command> trim_end(ClipId id, RationalTime new_end, bool ripple) {
    return detail::make_planned("Trim", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto end = tl.to_ticks(new_end);
        if (!c || !end) {
            return std::unexpected(!c ? c.error() : end.error());
        }
        auto trimmed = with_end(tl, **c, *end);
        if (!trimmed) {
            return std::unexpected(trimmed.error());
        }
        if (!ripple) {
            Steps steps;
            steps.push_back(detail::replace_clip(*trimmed));
            return steps;
        }
        return change_and_shift(*trimmed, tl.track_of(id)->id, (*c)->end_ticks(),
                                *end - (*c)->end_ticks());
    });
}

std::unique_ptr<Command> roll(ClipId left, RationalTime edit_point) {
    return detail::make_planned("Roll", [=](Timeline& tl) -> Result<Steps> {
        auto l = get_clip(tl, left);
        auto point = tl.to_ticks(edit_point);
        if (!l || !point) {
            return std::unexpected(!l ? l.error() : point.error());
        }
        const Track* track = tl.track_of(left);
        const Clip* r = tl.clip_at(track->id, (*l)->end_ticks());
        if (r == nullptr || r->start_ticks() != (*l)->end_ticks()) {
            return error(ErrorCode::InvalidArgument, "no clip starts at the end of this one",
                         detail::clip_context(left));
        }
        auto new_left = with_end(tl, **l, *point);
        auto new_right = with_start(tl, *r, *point);
        if (!new_left || !new_right) {
            return std::unexpected(!new_left ? new_left.error() : new_right.error());
        }
        Steps steps;
        auto change_left = detail::replace_clip(*new_left);
        auto change_right = detail::replace_clip(*new_right);
        // The clip that shrinks goes first so the other one has room.
        if (*point > (*l)->end_ticks()) {
            steps.push_back(std::move(change_right));
            steps.push_back(std::move(change_left));
        } else {
            steps.push_back(std::move(change_left));
            steps.push_back(std::move(change_right));
        }
        return steps;
    });
}

std::unique_ptr<Command> slip(ClipId id, RationalTime delta) {
    return detail::make_planned("Slip", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto d = tl.to_ticks(delta);
        if (!c || !d) {
            return std::unexpected(!c ? c.error() : d.error());
        }
        auto offset = (*c)->time_map.media_offset(*d, tl.timebase());
        if (!offset) {
            return std::unexpected(offset.error());
        }
        auto source_in = detail::add_exact((*c)->source_in, *offset);
        if (!source_in) {
            return std::unexpected(source_in.error());
        }
        Clip slipped = **c;
        slipped.source_in = *source_in;
        Steps steps;
        steps.push_back(detail::replace_clip(slipped));
        return steps;
    });
}

std::unique_ptr<Command> slide(ClipId id, RationalTime delta) {
    return detail::make_planned("Slide", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto d = tl.to_ticks(delta);
        if (!c || !d) {
            return std::unexpected(!c ? c.error() : d.error());
        }
        const Clip& clip = **c;
        const Track* track = tl.track_of(id);
        const Clip* prev =
            clip.start_ticks() > 0 ? tl.clip_at(track->id, clip.start_ticks() - 1) : nullptr;
        const Clip* next = tl.clip_at(track->id, clip.end_ticks());
        if (next != nullptr && next->start_ticks() != clip.end_ticks()) {
            next = nullptr;
        }
        Clip moved = clip;
        moved.start = tl.at(clip.start_ticks() + *d);
        if (moved.start.value() < 0) {
            return error(ErrorCode::InvalidArgument, "slide before time zero",
                         detail::clip_context(id));
        }
        std::unique_ptr<Command> change_prev;
        std::unique_ptr<Command> change_next;
        if (prev != nullptr) {
            auto p = with_end(tl, *prev, prev->end_ticks() + *d);
            if (!p) {
                return std::unexpected(p.error());
            }
            change_prev = detail::replace_clip(*p);
        }
        if (next != nullptr) {
            auto n = with_start(tl, *next, next->start_ticks() + *d);
            if (!n) {
                return std::unexpected(n.error());
            }
            change_next = detail::replace_clip(*n);
        }
        // Shrink the neighbour on the side the clip moves toward, move it, grow the other.
        Steps steps;
        auto& first = *d > 0 ? change_next : change_prev;
        auto& last = *d > 0 ? change_prev : change_next;
        if (first) {
            steps.push_back(std::move(first));
        }
        steps.push_back(detail::replace_clip(moved));
        if (last) {
            steps.push_back(std::move(last));
        }
        return steps;
    });
}

std::unique_ptr<Command> move_clip(ClipId id, TrackId track, RationalTime start) {
    return detail::make_planned("Move", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto t = get_track(tl, track);
        auto s = tl.to_ticks(start);
        if (!c || !t || !s) {
            return std::unexpected(!c ? c.error() : (!t ? t.error() : s.error()));
        }
        if (tl.track_of(id)->kind != (*t)->kind) {
            return error(ErrorCode::InvalidArgument, "tracks of different kinds",
                         detail::clip_context(id));
        }
        Clip moved = **c;
        moved.start = tl.at(*s);
        Steps steps;
        steps.push_back(detail::erase_clip(id));
        steps.push_back(detail::insert_clip(track, moved));
        return steps;
    });
}

std::unique_ptr<Command> set_speed(ClipId id, Rational speed, bool ripple) {
    return detail::make_planned("Speed", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        auto map = TimeMap::constant(speed);
        if (!c || !map) {
            return std::unexpected(!c ? c.error() : map.error());
        }
        auto length = (*c)->time_map.media_offset((*c)->duration.value(), tl.timebase());
        auto unit = detail::multiply(tl.timebase(), speed);
        if (!length || !unit) {
            return std::unexpected(!length ? length.error() : unit.error());
        }
        // Rounding down keeps the source range inside the media.
        auto ticks = rescale(length->value(), length->timebase(), *unit, Rounding::Floor);
        if (!ticks) {
            return std::unexpected(ticks.error());
        }
        if (*ticks <= 0) {
            return error(ErrorCode::InvalidArgument, "the clip would be empty",
                         detail::clip_context(id));
        }
        Clip changed = **c;
        changed.time_map = *map;
        changed.duration = tl.at(*ticks);
        if (!ripple) {
            Steps steps;
            steps.push_back(detail::replace_clip(changed));
            return steps;
        }
        return change_and_shift(changed, tl.track_of(id)->id, (*c)->end_ticks(),
                                *ticks - (*c)->duration.value());
    });
}

std::unique_ptr<Command> set_video(ClipId id, VideoProperties video) {
    return detail::make_planned("Video Adjustments", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        if (!c) {
            return std::unexpected(c.error());
        }
        Clip changed = **c;
        changed.video = video;
        Steps steps;
        steps.push_back(detail::replace_clip(changed));
        return steps;
    });
}

std::unique_ptr<Command> set_audio(ClipId id, AudioProperties audio) {
    return detail::make_planned("Audio Adjustments", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, id);
        if (!c) {
            return std::unexpected(c.error());
        }
        Clip changed = **c;
        changed.audio = audio;
        Steps steps;
        steps.push_back(detail::replace_clip(changed));
        return steps;
    });
}

std::unique_ptr<Command> detach_audio(ClipId video, TrackId lane, ClipId audio) {
    return detail::make_planned("Detach Audio", [=](Timeline& tl) -> Result<Steps> {
        auto c = get_clip(tl, video);
        auto t = get_track(tl, lane);
        if (!c || !t) {
            return std::unexpected(!c ? c.error() : t.error());
        }
        const MediaInfo* m = tl.find_media((*c)->media);
        if (tl.track_of(video)->kind != TrackKind::Video || m == nullptr || !m->has_audio ||
            (*c)->audio_detached) {
            return error(ErrorCode::InvalidArgument, "the clip has no sound to detach",
                         detail::clip_context(video));
        }
        if ((*t)->kind != TrackKind::Audio) {
            return error(ErrorCode::InvalidArgument, "sound detaches onto an audio track",
                         detail::track_context(lane));
        }
        Clip sound = **c;
        sound.id = audio;
        sound.video = {};
        Clip picture = **c;
        picture.audio_detached = true;
        Steps steps;
        steps.push_back(detail::insert_clip(lane, sound));
        steps.push_back(detail::replace_clip(picture));
        return steps;
    });
}

std::unique_ptr<Command> add_marker(MarkerId id, RationalTime at, std::string name) {
    return detail::make_planned(
        "Add Marker", [=, name = std::move(name)](Timeline& tl) -> Result<Steps> {
            auto ticks = tl.to_ticks(at);
            if (!ticks) {
                return std::unexpected(ticks.error());
            }
            Steps steps;
            steps.push_back(
                detail::insert_marker(Marker{.id = id, .time = tl.at(*ticks), .name = name}));
            return steps;
        });
}

std::unique_ptr<Command> remove_marker(MarkerId id) {
    return single("Remove Marker", detail::erase_marker(id));
}

std::unique_ptr<Command> transaction(std::string name,
                                     std::vector<std::unique_ptr<Command>> commands) {
    return detail::make_transaction(std::move(name), std::move(commands));
}

} // namespace oma::timeline::edit
