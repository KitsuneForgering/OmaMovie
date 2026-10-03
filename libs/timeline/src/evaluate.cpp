#include "oma/timeline/evaluate.hpp"

#include "mutation.hpp"

namespace oma::timeline {

namespace {

// The media time at `ticks` inside a clip, rounded down to the media's own timebase.
Result<RationalTime> media_time(const Timeline& tl, const Clip& c, const MediaInfo& m,
                                std::int64_t ticks) {
    auto offset = c.time_map.media_offset(ticks - c.start_ticks(), tl.timebase());
    if (!offset) {
        return std::unexpected(offset.error());
    }
    auto exact = detail::add_exact(c.source_in, *offset);
    if (!exact) {
        return std::unexpected(exact.error());
    }
    return exact->rescaled(m.start.timebase(), Rounding::Floor);
}

} // namespace

Result<Composition> evaluate(const Timeline& timeline, const RationalTime& at) {
    auto ticks = timeline.to_ticks(at);
    if (!ticks) {
        return std::unexpected(ticks.error());
    }
    Composition out{.time = timeline.at(*ticks), .video = {}, .audio = {}};
    for (const Track& track : timeline.tracks()) {
        const Clip* c = timeline.clip_at(track.id, *ticks);
        if (c == nullptr) {
            continue;
        }
        const MediaInfo* m = timeline.find_media(c->media);
        if (m == nullptr) {
            return detail::error(ErrorCode::InvalidData, "clip references unknown media",
                                 detail::clip_context(c->id));
        }
        auto t = media_time(timeline, *c, *m, *ticks);
        if (!t) {
            return std::unexpected(t.error());
        }
        if (track.kind == TrackKind::Video && !track.hidden) {
            out.video.push_back(
                VideoLayer{.clip = c->id, .media = c->media, .media_time = *t, .video = c->video});
        }
        const bool plays_audio =
            track.kind == TrackKind::Audio || (track.kind == TrackKind::Video && m->has_audio);
        if (plays_audio && !track.muted && !c->audio.muted) {
            out.audio.push_back(AudioSource{.clip = c->id,
                                            .media = c->media,
                                            .media_time = *t,
                                            .clip_offset = timeline.at(*ticks - c->start_ticks()),
                                            .clip_duration = c->duration,
                                            .speed = c->time_map.speed(),
                                            .audio = c->audio});
        }
    }
    return out;
}

} // namespace oma::timeline
