#include "oma/timeline/evaluate.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>

namespace oma::timeline {

Result<RationalTime> source_time(const Timeline& tl, const Clip& c, std::int64_t ticks) {
    auto offset = c.time_map.media_offset(ticks - c.start_ticks(), tl.timebase());
    if (!offset) {
        return std::unexpected(offset.error());
    }
    return detail::add_exact(c.source_in, *offset);
}

namespace {

// The media time at `ticks` inside a clip, in the media's own timebase: rounded down, so the
// frame shown is the one whose span contains the position. Playing backwards (ADR-0013) the
// position decreases through the instant, so the frame shown is the one ending there: one
// media tick before the position rounded up (otherwise a reversed clip would start on the
// frame after its range).
Result<RationalTime> media_time(const Timeline& tl, const Clip& c, const MediaInfo& m,
                                std::int64_t ticks) {
    auto exact = source_time(tl, c, ticks);
    if (!exact) {
        return std::unexpected(exact.error());
    }
    if (!c.time_map.backward(ticks - c.start_ticks())) {
        return exact->rescaled(m.start.timebase(), Rounding::Floor);
    }
    auto up = exact->rescaled(m.start.timebase(), Rounding::Ceil);
    if (!up) {
        return up;
    }
    return RationalTime::make(up->value() - 1, up->timebase());
}

// The clip's properties at `ticks`, keyframes evaluated (VideoLayer::video).
Result<VideoProperties> video_at(const Timeline& tl, const Clip& c, std::int64_t ticks) {
    VideoProperties v = c.video;
    if (!v.transform_keys.empty()) {
        auto source = source_time(tl, c, ticks);
        if (!source) {
            return std::unexpected(source.error());
        }
        v.transform = transform_at(c.video, *source);
        v.transform_keys.clear();
    }
    return v;
}

// Ticks of sequence time the media past (or before) a clip lasts at the clip's speed, rounded
// down; `available` is the media duration from the clip edge outward.
std::int64_t spare_ticks(const Timeline& tl, const Clip& c, const RationalTime& available) {
    // Segmented maps (ADR-0013) offer no handles: past their edges the motion is undefined.
    if (!c.time_map.is_constant()) {
        return 0;
    }
    auto unit = detail::multiply(tl.timebase(), c.time_map.speed());
    if (!unit || available.value() <= 0) {
        return 0;
    }
    auto ticks = rescale(available.value(), available.timebase(), *unit, Rounding::Floor);
    return ticks ? std::max<std::int64_t>(*ticks, 0) : 0;
}

RationalTime negated(const RationalTime& t) {
    return *RationalTime::make(-t.value(), t.timebase());
}

// Media a clip can show past its last instant.
std::int64_t spare_after(const Timeline& tl, const Clip& c) {
    if (c.title) {
        return std::numeric_limits<std::int64_t>::max(); // generated: no media edge
    }
    const MediaInfo* m = tl.find_media(c.media);
    if (m == nullptr) {
        return 0;
    }
    if (m->still) {
        return std::numeric_limits<std::int64_t>::max();
    }
    auto end = detail::source_end(c, tl.timebase());
    auto media_end = m->start.plus(m->duration);
    if (!end || !media_end) {
        return 0;
    }
    auto available = detail::add_exact(*media_end, negated(*end));
    return available ? spare_ticks(tl, c, *available) : 0;
}

// Media a clip can show before its first instant.
std::int64_t spare_before(const Timeline& tl, const Clip& c) {
    if (c.title) {
        return std::numeric_limits<std::int64_t>::max(); // generated: no media edge
    }
    const MediaInfo* m = tl.find_media(c.media);
    if (m == nullptr) {
        return 0;
    }
    if (m->still) {
        return std::numeric_limits<std::int64_t>::max();
    }
    auto available = detail::add_exact(c.source_in, negated(m->start));
    return available ? spare_ticks(tl, c, *available) : 0;
}

struct Mix {
    float from = 1.0F;
    float to = 1.0F;
    double reveal = 1.0;
};

Mix mix_of(TransitionKind kind, double p) {
    switch (kind) {
    case TransitionKind::DipToBlack:
        return p < 0.5
                   ? Mix{.from = static_cast<float>(1.0 - (2.0 * p)), .to = 0.0F, .reveal = 1.0}
                   : Mix{.from = 0.0F, .to = static_cast<float>((2.0 * p) - 1.0), .reveal = 1.0};
    case TransitionKind::Wipe:
        return {.from = 1.0F, .to = 1.0F, .reveal = p};
    case TransitionKind::Dissolve:
        break;
    }
    return {.from = 1.0F, .to = static_cast<float>(p), .reveal = 1.0};
}

} // namespace

double TransitionWindow::progress(std::int64_t ticks) const noexcept {
    if (half <= 0) {
        return 1.0;
    }
    const auto p = static_cast<double>(ticks - (cut - half)) / static_cast<double>(2 * half);
    return std::clamp(p, 0.0, 1.0);
}

std::optional<TransitionWindow> transition_window(const Timeline& timeline, const Track& track,
                                                  std::size_t index) {
    if (index == 0 || index >= track.clips.size()) {
        return std::nullopt;
    }
    const Clip& to = track.clips[index];
    const Clip& from = track.clips[index - 1];
    if (!to.transition_in || from.end_ticks() != to.start_ticks()) {
        return std::nullopt;
    }
    const std::int64_t half =
        std::min({to.transition_in->duration.value() / 2, spare_after(timeline, from),
                  spare_before(timeline, to), from.duration.value(), to.duration.value()});
    if (half <= 0) {
        return std::nullopt;
    }
    return TransitionWindow{.from = &from,
                            .to = &to,
                            .kind = to.transition_in->kind,
                            .cut = to.start_ticks(),
                            .half = half};
}

Transform transform_at(const VideoProperties& video, const RationalTime& source) {
    const auto& keys = video.transform_keys;
    if (keys.empty()) {
        return video.transform;
    }
    // O(log k): the first key after `source`.
    const auto next = std::ranges::upper_bound(keys, source, std::less{}, &TransformKey::at);
    if (next == keys.begin()) {
        return keys.front().value;
    }
    if (next == keys.end()) {
        return keys.back().value;
    }
    const TransformKey& a = *std::prev(next);
    const TransformKey& b = *next;
    if (a.interpolation == Interpolation::Hold) {
        return a.value;
    }
    // A display-derived fraction; the key times themselves stay exact.
    const double span = b.at.seconds_approx() - a.at.seconds_approx();
    double t = std::clamp((source.seconds_approx() - a.at.seconds_approx()) / span, 0.0, 1.0);
    if (a.interpolation == Interpolation::Ease) {
        t = t * t * (3.0 - (2.0 * t));
    }
    const auto lerp = [t](double x, double y) {
        return x + ((y - x) * t);
    };
    const auto scale = [&](double x, double y) {
        return x * y > 0.0 ? x * std::pow(y / x, t) : lerp(x, y);
    };
    const Transform& x = a.value;
    const Transform& y = b.value;
    return {.offset_x = lerp(x.offset_x, y.offset_x),
            .offset_y = lerp(x.offset_y, y.offset_y),
            .scale_x = scale(x.scale_x, y.scale_x),
            .scale_y = scale(x.scale_y, y.scale_y),
            .rotation = lerp(x.rotation, y.rotation)};
}

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
        const MediaInfo* m = c->title ? nullptr : timeline.find_media(c->media);
        if (m == nullptr && !c->title) {
            return detail::error(ErrorCode::InvalidData, "clip references unknown media",
                                 detail::clip_context(c->id));
        }
        // A title has no media time; its picture does not depend on one.
        auto t = m != nullptr ? media_time(timeline, *c, *m, *ticks)
                              : Result<RationalTime>(RationalTime{});
        if (!t) {
            return std::unexpected(t.error());
        }
        if (track.kind == TrackKind::Video && !track.hidden) {
            // A transition window around the cut at this clip's start or end, if any covers t.
            const auto index = static_cast<std::size_t>(c - track.clips.data());
            std::optional<TransitionWindow> window = transition_window(timeline, track, index);
            if (window && *ticks >= window->cut + window->half) {
                window.reset();
            }
            if (!window) {
                window = transition_window(timeline, track, index + 1);
                if (window && *ticks < window->cut - window->half) {
                    window.reset();
                }
            }
            if (!window) {
                auto video = video_at(timeline, *c, *ticks);
                if (!video) {
                    return std::unexpected(video.error());
                }
                out.video.push_back(VideoLayer{.clip = c->id,
                                               .media = c->media,
                                               .media_time = *t,
                                               .video = *video,
                                               .opacity = 1.0F,
                                               .reveal = 1.0,
                                               .title = c->title});
            } else {
                const Mix mix = mix_of(window->kind, window->progress(*ticks));
                for (const Clip* layer : {window->from, window->to}) {
                    const bool incoming = layer == window->to;
                    const float opacity = incoming ? mix.to : mix.from;
                    if (opacity <= 0.0F) {
                        continue;
                    }
                    const MediaInfo* lm =
                        layer->title ? nullptr : timeline.find_media(layer->media);
                    auto lt = layer->title    ? Result<RationalTime>(RationalTime{})
                              : lm != nullptr ? media_time(timeline, *layer, *lm, *ticks)
                                              : Result<RationalTime>(std::unexpected(Error(
                                                    ErrorCode::InvalidData, Category::Timeline,
                                                    "clip references unknown media")));
                    if (!lt) {
                        return std::unexpected(lt.error());
                    }
                    auto video = video_at(timeline, *layer, *ticks);
                    if (!video) {
                        return std::unexpected(video.error());
                    }
                    out.video.push_back(VideoLayer{.clip = layer->id,
                                                   .media = layer->media,
                                                   .media_time = *lt,
                                                   .video = *video,
                                                   .opacity = opacity,
                                                   .reveal = incoming ? mix.reveal : 1.0,
                                                   .title = layer->title});
                }
            }
        }
        const bool plays_audio =
            m != nullptr &&
            (track.kind == TrackKind::Audio ||
             (track.kind == TrackKind::Video && m->has_audio && !c->audio_detached));
        if (plays_audio && !track.muted && !c->audio.muted) {
            out.audio.push_back(AudioSource{
                .clip = c->id,
                .media = c->media,
                .media_time = *t,
                .clip_offset = timeline.at(*ticks - c->start_ticks()),
                .clip_duration = c->duration,
                .speed = c->time_map.is_constant() ? c->time_map.speed() : Rational::literal(0, 1),
                .audio = c->audio});
        }
    }
    return out;
}

} // namespace oma::timeline
