#include "oma/timeline/model.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
#include <unordered_set>

namespace oma::timeline {

using detail::error;

Result<TimeMap> TimeMap::constant(Rational speed) {
    if (!speed.is_positive()) {
        return error(ErrorCode::InvalidArgument, "speed must be positive",
                     std::format("{}/{}", speed.num(), speed.den()));
    }
    return TimeMap(speed);
}

Result<RationalTime> TimeMap::media_offset(std::int64_t local_ticks,
                                           Rational sequence_timebase) const {
    auto unit = detail::multiply(sequence_timebase, speed_);
    if (!unit) {
        return std::unexpected(unit.error());
    }
    return RationalTime::make(local_ticks, *unit);
}

Result<Timeline> Timeline::create(FrameRate rate, Rational timebase) {
    if (!timebase.is_positive()) {
        return error(ErrorCode::InvalidArgument, "sequence timebase must be positive");
    }
    // Every frame boundary must be a whole number of ticks.
    auto frame = rescale(1, rate.frame_duration(), timebase, Rounding::Floor);
    if (!frame || *frame <= 0 ||
        *RationalTime::make(*frame, timebase) != *RationalTime::make(1, rate.frame_duration())) {
        return error(ErrorCode::InvalidArgument,
                     "the frame duration is not a whole number of sequence ticks",
                     std::format("{}/{} fps, timebase {}/{}", rate.fps().num(), rate.fps().den(),
                                 timebase.num(), timebase.den()));
    }
    return Timeline(rate, timebase);
}

Result<Timeline> Timeline::create(FrameRate rate, SampleRate audio_rate) {
    auto timebase = default_timebase(rate, audio_rate);
    if (!timebase) {
        return std::unexpected(timebase.error());
    }
    return create(rate, *timebase);
}

Result<Rational> Timeline::default_timebase(FrameRate rate, SampleRate audio_rate) {
    // A frame lasts den/num seconds, a sample 1/hz: 1/lcm(num, hz) divides both.
    const std::int64_t num = rate.fps().num();
    const std::int64_t hz = audio_rate.hz();
    const std::int64_t g = std::gcd(num, hz);
    std::int64_t lcm = 0;
    if (__builtin_mul_overflow(num / g, hz, &lcm)) {
        return error(ErrorCode::Overflow, "no common sequence timebase");
    }
    return Rational::make(1, lcm);
}

const Track* Timeline::find_track(TrackId id) const noexcept {
    const auto it = std::ranges::find(tracks_, id, &Track::id);
    return it == tracks_.end() ? nullptr : &*it;
}

const Clip* Timeline::find_clip(ClipId id) const noexcept {
    for (const Track& t : tracks_) {
        const auto it = std::ranges::find(t.clips, id, &Clip::id);
        if (it != t.clips.end()) {
            return &*it;
        }
    }
    return nullptr;
}

const Track* Timeline::track_of(ClipId id) const noexcept {
    for (const Track& t : tracks_) {
        if (std::ranges::find(t.clips, id, &Clip::id) != t.clips.end()) {
            return &t;
        }
    }
    return nullptr;
}

const MediaInfo* Timeline::find_media(MediaId id) const noexcept {
    const auto it = std::ranges::find(media_, id, &MediaInfo::id);
    return it == media_.end() ? nullptr : &*it;
}

const Marker* Timeline::find_marker(MarkerId id) const noexcept {
    const auto it = std::ranges::find(markers_, id, &Marker::id);
    return it == markers_.end() ? nullptr : &*it;
}

const Clip* Timeline::clip_at(TrackId track, std::int64_t ticks) const noexcept {
    const Track* t = find_track(track);
    if (t == nullptr) {
        return nullptr;
    }
    // Clips are sorted by start: the candidate is the last one starting at or before `ticks`.
    const auto it = std::ranges::upper_bound(t->clips, ticks, {}, &Clip::start_ticks);
    if (it == t->clips.begin()) {
        return nullptr;
    }
    const Clip& c = *std::prev(it);
    return ticks < c.end_ticks() ? &c : nullptr;
}

Result<std::int64_t> Timeline::to_ticks(const RationalTime& t) const {
    auto ticks = rescale(t.value(), t.timebase(), timebase_, Rounding::Floor);
    if (!ticks) {
        return std::unexpected(ticks.error());
    }
    if (at(*ticks) != t) {
        return error(ErrorCode::InvalidArgument, "time is not on the sequence grid",
                     std::format("{} @ {}/{}", t.value(), t.timebase().num(), t.timebase().den()));
    }
    return *ticks;
}

RationalTime Timeline::at(std::int64_t ticks) const noexcept {
    return *RationalTime::make(ticks, timebase_);
}

RationalTime Timeline::duration() const noexcept {
    std::int64_t end = 0;
    for (const Track& t : tracks_) {
        if (!t.clips.empty()) {
            end = std::max(end, t.clips.back().end_ticks());
        }
    }
    return at(end);
}

namespace {

bool in_unit(double v) {
    return std::isfinite(v) && v >= 0.0 && v < 1.0;
}

Result<void> validate_properties(const Clip& c) {
    const VideoProperties& v = c.video;
    const bool crop_ok = in_unit(v.crop.left) && in_unit(v.crop.right) && in_unit(v.crop.top) &&
                         in_unit(v.crop.bottom) && v.crop.left + v.crop.right < 1.0 &&
                         v.crop.top + v.crop.bottom < 1.0;
    const Transform& tr = v.transform;
    const bool transform_ok = std::isfinite(tr.offset_x) && std::isfinite(tr.offset_y) &&
                              std::isfinite(tr.rotation) && std::isfinite(tr.scale_x) &&
                              std::isfinite(tr.scale_y) && tr.scale_x != 0.0 && tr.scale_y != 0.0;
    const bool opacity_ok = std::isfinite(v.opacity) && v.opacity >= 0.0F && v.opacity <= 1.0F;
    if (!crop_ok || !transform_ok || !opacity_ok) {
        return error(ErrorCode::InvalidData, "invalid video properties",
                     detail::clip_context(c.id));
    }
    const AudioProperties& a = c.audio;
    if (!std::isfinite(a.gain) || a.gain < 0.0F) {
        return error(ErrorCode::InvalidData, "invalid audio gain", detail::clip_context(c.id));
    }
    const RationalTime zero;
    auto fades = detail::add_exact(a.fade_in, a.fade_out);
    if (a.fade_in < zero || a.fade_out < zero || !fades || c.duration < *fades) {
        return error(ErrorCode::InvalidData, "fades do not fit in the clip",
                     detail::clip_context(c.id));
    }
    return {};
}

Result<void> validate_source(const Clip& c, const MediaInfo& m, Rational timebase) {
    if (m.still) {
        return {};
    }
    auto end = detail::source_end(c, timebase);
    if (!end) {
        return std::unexpected(end.error());
    }
    auto media_end = m.start.plus(m.duration);
    if (!media_end) {
        return std::unexpected(media_end.error());
    }
    if (c.source_in < m.start || *media_end < *end) {
        return error(ErrorCode::InvalidData, "source range outside the media",
                     detail::clip_context(c.id));
    }
    return {};
}

bool media_fits_track(const MediaInfo& m, TrackKind kind) {
    switch (kind) {
    case TrackKind::Video:
        return m.has_video;
    case TrackKind::Audio:
        return m.has_audio;
    case TrackKind::Caption:
        return false; // caption clips are not media clips (v0.2)
    }
    return false;
}

} // namespace

Result<void> Timeline::validate() const {
    std::unordered_set<MediaId> media_ids;
    for (const MediaInfo& m : media_) {
        if (!m.id.valid() || !media_ids.insert(m.id).second) {
            return error(ErrorCode::InvalidData, "invalid or duplicate media ID",
                         std::format("media {}", m.id.value()));
        }
        if (!m.still && (m.duration.value() < 0 || m.start.timebase() != m.duration.timebase())) {
            return error(ErrorCode::InvalidData, "invalid media range",
                         std::format("media {}", m.id.value()));
        }
    }

    std::unordered_set<TrackId> track_ids;
    std::unordered_set<ClipId> clip_ids;
    for (const Track& t : tracks_) {
        if (!t.id.valid() || !track_ids.insert(t.id).second) {
            return error(ErrorCode::InvalidData, "invalid or duplicate track ID",
                         detail::track_context(t.id));
        }
        std::int64_t previous_end = 0;
        for (const Clip& c : t.clips) {
            if (!c.id.valid() || !clip_ids.insert(c.id).second) {
                return error(ErrorCode::InvalidData, "invalid or duplicate clip ID",
                             detail::clip_context(c.id));
            }
            if (c.start.timebase() != timebase_ || c.duration.timebase() != timebase_) {
                return error(ErrorCode::InvalidData, "clip off the sequence timebase",
                             detail::clip_context(c.id));
            }
            if (c.duration.value() <= 0 || c.start.value() < previous_end) {
                return error(ErrorCode::InvalidData, "empty, overlapping or unsorted clip",
                             std::format("{} on {}", detail::clip_context(c.id),
                                         detail::track_context(t.id)));
            }
            auto end = detail::add_ticks(c.start.value(), c.duration.value());
            if (!end) {
                return std::unexpected(end.error());
            }
            previous_end = *end;
            const MediaInfo* m = find_media(c.media);
            if (m == nullptr) {
                return error(ErrorCode::InvalidData, "clip references unknown media",
                             detail::clip_context(c.id));
            }
            if (c.audio_detached && t.kind != TrackKind::Video) {
                return error(ErrorCode::InvalidData, "only video clips detach their audio",
                             detail::clip_context(c.id));
            }
            if (!media_fits_track(*m, t.kind)) {
                return error(ErrorCode::InvalidData, "media kind does not fit the track",
                             std::format("{} on {}", detail::clip_context(c.id),
                                         detail::track_context(t.id)));
            }
            if (auto r = validate_source(c, *m, timebase_); !r) {
                return r;
            }
            if (auto r = validate_properties(c); !r) {
                return r;
            }
        }
    }

    std::unordered_set<MarkerId> marker_ids;
    for (const Marker& m : markers_) {
        if (!m.id.valid() || !marker_ids.insert(m.id).second || m.time.timebase() != timebase_ ||
            m.time.value() < 0) {
            return error(ErrorCode::InvalidData, "invalid marker",
                         std::format("marker {}", m.id.value()));
        }
    }
    return {};
}

} // namespace oma::timeline
