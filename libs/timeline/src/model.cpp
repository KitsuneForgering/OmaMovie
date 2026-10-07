#include "oma/timeline/model.hpp"

#include "oma/timeline/effects.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
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

namespace {

bool is_zero(Rational r) noexcept {
    return r.num() == 0;
}

// Ticks of displacement (media ticks at speed 1) -> media time in seconds as a RationalTime.
Result<RationalTime> to_time(Rational ticks, Rational sequence_timebase) {
    auto seconds = detail::multiply(ticks, sequence_timebase);
    if (!seconds) {
        return std::unexpected(seconds.error());
    }
    auto unit = Rational::make(1, seconds->den());
    if (!unit) {
        return std::unexpected(unit.error());
    }
    return RationalTime::make(seconds->num(), *unit);
}

} // namespace

Result<TimeMap> TimeMap::segmented(std::vector<TimeSegment> segments) {
    if (segments.empty() || segments.size() > kMaxTimeSegments) {
        return error(ErrorCode::InvalidArgument, "a segmented time map needs 1 to 256 segments");
    }
    std::int64_t total = 0;
    for (const TimeSegment& s : segments) {
        bool valid = s.length > 0 && !__builtin_add_overflow(total, s.length, &total);
        switch (s.kind) {
        case TimeSegment::Kind::Linear:
            valid = valid && !is_zero(s.from);
            break;
        case TimeSegment::Kind::Freeze:
            break;
        case TimeSegment::Kind::Ramp:
            // Monotonic: never changes direction, and not a freeze in disguise.
            // Same sign or one end zero: the product of the signs is never negative.
            valid = valid && (!is_zero(s.from) || !is_zero(s.to)) &&
                    (s.from.num() > 0 ? 1 : (s.from.num() < 0 ? -1 : 0)) *
                            (s.to.num() > 0 ? 1 : (s.to.num() < 0 ? -1 : 0)) >=
                        0;
            break;
        default:
            valid = false;
        }
        if (!valid) {
            return error(ErrorCode::InvalidArgument, "invalid time segment",
                         std::format("length {}", s.length));
        }
    }
    TimeMap map;
    map.segments_ = std::move(segments);
    return map;
}

std::int64_t TimeMap::length() const noexcept {
    std::int64_t total = 0;
    for (const TimeSegment& s : segments_)
        total += s.length; // checked in segmented()
    return total;
}

bool TimeMap::backward(std::int64_t t) const noexcept {
    std::int64_t start = 0;
    for (const TimeSegment& s : segments_) {
        if (t < start + s.length || &s == &segments_.back()) {
            // A ramp never changes direction: either end's sign is its direction.
            return s.from.num() < 0 || (s.kind == TimeSegment::Kind::Ramp && s.to.num() < 0);
        }
        start += s.length;
    }
    return false;
}

Result<RationalTime> TimeMap::media_offset(std::int64_t local_ticks,
                                           Rational sequence_timebase) const {
    if (is_constant()) {
        auto unit = detail::multiply(sequence_timebase, speed_);
        if (!unit) {
            return std::unexpected(unit.error());
        }
        return RationalTime::make(local_ticks, *unit);
    }
    // O(segments): clips have a handful (ramp presets), so no prefix index is kept.
    Rational moved = Rational::literal(0, 1);
    std::int64_t left = local_ticks;
    for (std::size_t i = 0; i < segments_.size() && left > 0; ++i) {
        const TimeSegment& s = segments_[i];
        // The last segment extends past its length only for the clip's end instant.
        const std::int64_t t = (i + 1 == segments_.size()) ? left : std::min(left, s.length);
        auto d = detail::displacement(s, t);
        if (!d) {
            return std::unexpected(d.error());
        }
        auto next = detail::add(moved, *d);
        if (!next) {
            return std::unexpected(next.error());
        }
        moved = *next;
        left -= t;
    }
    return to_time(moved, sequence_timebase);
}

Result<std::pair<RationalTime, RationalTime>> TimeMap::extent(std::int64_t duration,
                                                              Rational sequence_timebase) const {
    auto low = media_offset(0, sequence_timebase);
    if (!low) {
        return std::unexpected(low.error());
    }
    RationalTime high = *low;
    // Each segment is monotonic, so the extremes are at segment boundaries or the end.
    std::int64_t at = 0;
    const auto consider = [&](std::int64_t ticks) -> Result<void> {
        auto o = media_offset(ticks, sequence_timebase);
        if (!o) {
            return std::unexpected(o.error());
        }
        low = std::min(*low, *o);
        high = std::max(high, *o);
        return {};
    };
    for (const TimeSegment& s : segments_) {
        at += s.length;
        if (at >= duration)
            break;
        if (auto r = consider(at); !r) {
            return std::unexpected(r.error());
        }
    }
    if (auto r = consider(duration); !r) {
        return std::unexpected(r.error());
    }
    return std::pair{*low, high};
}

Result<TimeMap> TimeMap::slice(std::int64_t from, std::int64_t to) const {
    if (from < 0 || to <= from) {
        return error(ErrorCode::InvalidArgument, "invalid time map slice",
                     std::format("[{}, {})", from, to));
    }
    if (is_constant()) {
        return *this;
    }
    std::vector<TimeSegment> out;
    std::int64_t start = 0;
    for (const TimeSegment& s : segments_) {
        const std::int64_t end = start + s.length;
        const std::int64_t a = std::max(from, start);
        const std::int64_t b = std::min(to, end);
        if (a < b) {
            TimeSegment piece = s;
            piece.length = b - a;
            if (s.kind == TimeSegment::Kind::Ramp) {
                // The exact speeds at the cut points keep the slice's motion identical.
                auto f = detail::speed_at(s, a - start);
                auto t = detail::speed_at(s, b - start);
                if (!f || !t) {
                    return std::unexpected(!f ? f.error() : t.error());
                }
                piece.from = *f;
                piece.to = *t;
                if (is_zero(*f) && is_zero(*t)) {
                    piece.kind = TimeSegment::Kind::Freeze;
                } else if (*f == *t) {
                    piece.kind = TimeSegment::Kind::Linear;
                }
            }
            out.push_back(piece);
        }
        start = end;
    }
    if (out.empty() || to > start) {
        return error(ErrorCode::InvalidArgument, "slice outside the time map",
                     std::format("[{}, {}) of {}", from, to, start));
    }
    return segmented(std::move(out));
}

namespace {

// A segment continuing at `speed` for `length` ticks.
TimeSegment steady(Rational speed, std::int64_t length) {
    return speed.num() == 0 ? TimeSegment{.kind = TimeSegment::Kind::Freeze,
                                          .length = length,
                                          .from = speed,
                                          .to = speed}
                            : TimeSegment{.kind = TimeSegment::Kind::Linear,
                                          .length = length,
                                          .from = speed,
                                          .to = speed};
}

Rational negated(Rational r) {
    return *Rational::make(-r.num(), r.den());
}

} // namespace

Result<TimeMap> TimeMap::extended(std::int64_t before, std::int64_t after) const {
    if (before < 0 || after < 0) {
        return error(ErrorCode::InvalidArgument, "negative time map extension");
    }
    if (is_constant() || (before == 0 && after == 0)) {
        return *this;
    }
    std::vector<TimeSegment> out = segments_;
    const auto grow = [](std::vector<TimeSegment>& v, bool front, std::int64_t by) -> Result<void> {
        TimeSegment& edge = front ? v.front() : v.back();
        if (edge.kind != TimeSegment::Kind::Ramp) {
            if (__builtin_add_overflow(edge.length, by, &edge.length)) {
                return error(ErrorCode::Overflow, "time map extension overflow");
            }
            return {};
        }
        const TimeSegment piece = steady(front ? edge.from : edge.to, by);
        v.insert(front ? v.begin() : v.end(), piece);
        return {};
    };
    if (before > 0) {
        if (auto r = grow(out, true, before); !r) {
            return std::unexpected(r.error());
        }
    }
    if (after > 0) {
        if (auto r = grow(out, false, after); !r) {
            return std::unexpected(r.error());
        }
    }
    return segmented(std::move(out));
}

Result<TimeMap> TimeMap::as_segments(std::int64_t duration) const {
    if (!is_constant()) {
        return *this;
    }
    return segmented({steady(speed_, duration)});
}

Result<TimeMap> TimeMap::reversed(std::int64_t duration) const {
    auto segs = as_segments(duration);
    if (!segs) {
        return segs;
    }
    std::vector<TimeSegment> out(segs->segments_.rbegin(), segs->segments_.rend());
    for (TimeSegment& s : out) {
        const Rational from = s.from;
        s.from = negated(s.to);
        s.to = negated(from);
    }
    return segmented(std::move(out));
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

Result<Timeline> Timeline::restore(FrameRate rate, Rational timebase, std::vector<Track> tracks,
                                   std::vector<Marker> markers, std::vector<MediaInfo> media,
                                   std::vector<LutInfo> luts, std::vector<Caption> captions) {
    auto t = create(rate, timebase);
    if (!t) {
        return t;
    }
    std::uint64_t largest = 0;
    for (const Track& track : tracks) {
        largest = std::max(largest, track.id.value());
        for (const Clip& c : track.clips) {
            largest = std::max(largest, c.id.value());
        }
    }
    for (const Marker& m : markers) {
        largest = std::max(largest, m.id.value());
    }
    for (const Caption& c : captions) {
        largest = std::max(largest, c.id.value());
    }
    if (largest == std::numeric_limits<std::uint64_t>::max()) {
        return error(ErrorCode::InvalidData, "IDs exhausted");
    }
    t->tracks_ = std::move(tracks);
    t->markers_ = std::move(markers);
    t->captions_ = std::move(captions);
    t->media_ = std::move(media);
    t->luts_ = std::move(luts);
    t->next_id_ = largest + 1;
    if (auto valid = t->validate(); !valid) {
        return std::unexpected(valid.error());
    }
    return t;
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

const LutInfo* Timeline::find_lut(LutId id) const noexcept {
    const auto it = std::ranges::find(luts_, id, &LutInfo::id);
    return it == luts_.end() ? nullptr : &*it;
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

bool curve_ok(const std::vector<CurvePoint>& points) {
    if (points.empty()) {
        return true;
    }
    if (points.size() < 2 || points.size() > kMaxCurvePoints) {
        return false;
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        const CurvePoint& p = points[i];
        const bool inside = std::isfinite(p.x) && std::isfinite(p.y) && p.x >= 0.0 && p.x <= 1.0 &&
                            p.y >= 0.0 && p.y <= 1.0;
        if (!inside || (i > 0 && p.x <= points[i - 1].x)) {
            return false;
        }
    }
    return true;
}

bool grade_ok(const ColorGrade& g) {
    const auto between = [](double x, double lo, double hi) {
        return std::isfinite(x) && x >= lo && x <= hi;
    };
    for (std::size_t i = 0; i < 3; ++i) {
        if (!between(g.cdl.slope[i], 0.0, 4.0) || !between(g.cdl.offset[i], -1.0, 1.0) ||
            !between(g.cdl.power[i], 0.1, 4.0)) {
            return false;
        }
    }
    return between(g.cdl.saturation, 0.0, 4.0) && between(g.lut_amount, 0.0, 1.0) &&
           curve_ok(g.curves.master) && curve_ok(g.curves.red) && curve_ok(g.curves.green) &&
           curve_ok(g.curves.blue);
}

bool transform_ok(const Transform& tr) {
    return std::isfinite(tr.offset_x) && std::isfinite(tr.offset_y) && std::isfinite(tr.rotation) &&
           std::isfinite(tr.scale_x) && std::isfinite(tr.scale_y) && tr.scale_x != 0.0 &&
           tr.scale_y != 0.0;
}

bool keys_ok(const std::vector<TransformKey>& keys) {
    if (keys.size() > kMaxKeys) {
        return false;
    }
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (!transform_ok(keys[i].value) || keys[i].interpolation > Interpolation::Ease ||
            (i > 0 && !(keys[i - 1].at < keys[i].at))) {
            return false;
        }
    }
    return true;
}

Result<void> validate_properties(const Clip& c) {
    const VideoProperties& v = c.video;
    const bool crop_ok = in_unit(v.crop.left) && in_unit(v.crop.right) && in_unit(v.crop.top) &&
                         in_unit(v.crop.bottom) && v.crop.left + v.crop.right < 1.0 &&
                         v.crop.top + v.crop.bottom < 1.0;
    const bool motion_ok = transform_ok(v.transform) && keys_ok(v.transform_keys);
    const bool opacity_ok = std::isfinite(v.opacity) && v.opacity >= 0.0F && v.opacity <= 1.0F;
    const auto within = [](double x, double limit) {
        return std::isfinite(x) && std::abs(x) <= limit;
    };
    const bool color_ok = within(v.color.exposure, 4.0) && within(v.color.contrast, 1.0) &&
                          within(v.color.saturation, 1.0) && within(v.color.temperature, 1.0);
    if (!crop_ok || !motion_ok || !opacity_ok || !color_ok || !grade_ok(v.grade)) {
        return error(ErrorCode::InvalidData, "invalid video properties",
                     detail::clip_context(c.id));
    }
    if (auto r = validate_effects(v.effects); !r) {
        return error(ErrorCode::InvalidData, r.error().message(), detail::clip_context(c.id));
    }
    const AudioProperties& a = c.audio;
    if (!std::isfinite(a.gain) || a.gain < 0.0F) {
        return error(ErrorCode::InvalidData, "invalid audio gain", detail::clip_context(c.id));
    }
    const auto band_ok = [](float db) {
        return std::isfinite(db) && std::abs(db) <= 24.0F;
    };
    if (!band_ok(a.eq.low_db) || !band_ok(a.eq.mid_db) || !band_ok(a.eq.high_db)) {
        return error(ErrorCode::InvalidData, "equalizer gain outside ±24 dB",
                     detail::clip_context(c.id));
    }
    if (!std::isfinite(a.noise.amount) || a.noise.amount < 0.0F || a.noise.amount > 1.0F ||
        !std::isfinite(a.noise.floor_db) || a.noise.floor_db < -120.0F || a.noise.floor_db > 0.0F) {
        return error(ErrorCode::InvalidData, "invalid noise reduction", detail::clip_context(c.id));
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
    if (!c.time_map.is_constant() && c.time_map.length() != c.duration.value()) {
        return error(ErrorCode::InvalidData, "time map does not cover the clip",
                     detail::clip_context(c.id));
    }
    if (m.still) {
        return {};
    }
    // Reverse and freeze segments may go below source_in: check the whole span used.
    auto span = c.time_map.extent(c.duration.value(), timebase);
    if (!span) {
        return std::unexpected(span.error());
    }
    auto first = detail::add_exact(c.source_in, span->first);
    auto end = detail::add_exact(c.source_in, span->second);
    if (!first || !end) {
        return std::unexpected(!first ? first.error() : end.error());
    }
    auto media_end = m.start.plus(m.duration);
    if (!media_end) {
        return std::unexpected(media_end.error());
    }
    if (*first < m.start || *media_end < *end) {
        return error(ErrorCode::InvalidData, "source range outside the media",
                     detail::clip_context(c.id));
    }
    return {};
}

// ADR-0014: the primary exists on another track, is not connected itself, plays at a constant
// speed, shows the anchor's source, and the dependent starts exactly where it does.
// ponytail: find_clip/track_of are linear, so this is O(clips) per connected clip; index the
// clips if the long-form gate shows validation cost.
Result<void> validate_anchor(const Timeline& tl, const Track& track, const Clip& c,
                             const Anchor& anchor) {
    const Clip* p = tl.find_clip(anchor.primary);
    const Track* pt = p != nullptr ? tl.track_of(p->id) : nullptr;
    bool valid = p != nullptr && pt != nullptr && pt->id != track.id && !p->anchor &&
                 p->time_map.is_constant() && detail::holds(*p, anchor.source, tl.timebase());
    if (valid) {
        auto at = detail::attach_tick(*p, anchor.source, tl.timebase());
        valid = at && *at == c.start_ticks();
    }
    if (!valid) {
        return error(ErrorCode::InvalidData, "invalid connection to a primary clip",
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

// A title clip (ADR-0015): no media, a video track, 1×, sound-free, sane text parameters.
Result<void> validate_title(const Clip& c, const Title& title, const Track& t) {
    const auto unit = [](float v) {
        return std::isfinite(v) && v >= 0.0F && v <= 1.0F;
    };
    const bool valid = t.kind == TrackKind::Video && !c.media.valid() && c.time_map.is_constant() &&
                       c.time_map.speed() == Rational::literal(1, 1) && !c.audio_detached &&
                       title.text.size() <= kMaxTitleBytes && title.font.size() <= 256 &&
                       std::isfinite(title.size) && title.size > 0.0 && title.size <= 0.5 &&
                       std::ranges::all_of(title.color, unit) &&
                       title.placement <= TitlePlacement::Top;
    if (!valid) {
        return error(ErrorCode::InvalidData, "invalid title clip", detail::clip_context(c.id));
    }
    return {};
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

    std::unordered_set<LutId> lut_ids;
    for (const LutInfo& l : luts_) {
        if (!l.id.valid() || !lut_ids.insert(l.id).second) {
            return error(ErrorCode::InvalidData, "invalid or duplicate LUT ID",
                         std::format("LUT {}", l.id.value()));
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
            const MediaInfo* m = c.title ? nullptr : find_media(c.media);
            if (c.title) {
                if (auto r = validate_title(c, *c.title, t); !r) {
                    return r;
                }
            } else if (m == nullptr) {
                return error(ErrorCode::InvalidData, "clip references unknown media",
                             detail::clip_context(c.id));
            }
            if (const auto& tr = c.transition_in;
                tr && (t.kind == TrackKind::Caption || tr->kind > TransitionKind::Wipe ||
                       tr->duration.timebase() != timebase_ || tr->duration.value() <= 0)) {
                return error(ErrorCode::InvalidData, "invalid transition",
                             detail::clip_context(c.id));
            }
            if (c.audio_detached && t.kind != TrackKind::Video) {
                return error(ErrorCode::InvalidData, "only video clips detach their audio",
                             detail::clip_context(c.id));
            }
            if (m != nullptr && !media_fits_track(*m, t.kind)) {
                return error(ErrorCode::InvalidData, "media kind does not fit the track",
                             std::format("{} on {}", detail::clip_context(c.id),
                                         detail::track_context(t.id)));
            }
            if (m != nullptr) {
                if (auto r = validate_source(c, *m, timebase_); !r) {
                    return r;
                }
            }
            if (auto r = validate_properties(c); !r) {
                return r;
            }
            if (c.anchor) {
                if (auto r = validate_anchor(*this, t, c, *c.anchor); !r) {
                    return r;
                }
            }
            if (c.video.grade.lut.valid() && find_lut(c.video.grade.lut) == nullptr) {
                return error(ErrorCode::InvalidData, "clip grades with an unknown LUT",
                             detail::clip_context(c.id));
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
    if (captions_.size() > kMaxCaptions) {
        return error(ErrorCode::InvalidData, "too many captions");
    }
    std::unordered_set<CaptionId> caption_ids;
    std::int64_t previous_end = 0;
    for (const Caption& c : captions_) {
        const bool timed =
            c.start.timebase() == timebase_ && c.duration.timebase() == timebase_ &&
            c.start.value() >= 0 && c.duration.value() > 0 &&
            c.start.value() <= std::numeric_limits<std::int64_t>::max() - c.duration.value();
        if (!c.id.valid() || !caption_ids.insert(c.id).second || !timed ||
            c.start.value() < previous_end || c.text.size() > kMaxCaptionBytes) {
            return error(ErrorCode::InvalidData, "invalid or overlapping caption",
                         std::format("caption {}", c.id.value()));
        }
        previous_end = c.start.value() + c.duration.value();
    }
    return {};
}

} // namespace oma::timeline
