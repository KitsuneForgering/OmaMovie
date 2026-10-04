#include "oma/playback/timeline_audio.hpp"

#include "clip_effects.hpp"

#include "oma/audio/mix.hpp"
#include "oma/timeline/evaluate.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace oma::playback {

namespace tl = oma::timeline;

namespace {

__extension__ using Int128 = __int128;

Int128 floor_div(Int128 a, Int128 b) {
    const Int128 q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

std::int64_t ceil_div(std::int64_t a, std::int64_t b) {
    return -static_cast<std::int64_t>(floor_div(-static_cast<Int128>(a), b));
}

// floor((t + ticks * unit) * rate): the media sample at a time offset, exact (128-bit).
std::int64_t sample_at(const oma::RationalTime& t, std::int64_t ticks, oma::Rational unit,
                       std::int32_t rate) {
    const oma::Rational tb = t.timebase();
    const Int128 den = static_cast<Int128>(tb.den()) * unit.den();
    const Int128 num = (static_cast<Int128>(t.value()) * tb.num() * unit.den() +
                        static_cast<Int128>(ticks) * unit.num() * tb.den()) *
                       rate;
    return static_cast<std::int64_t>(floor_div(num, den));
}

} // namespace

TimelineAudio::TimelineAudio(tl::Timeline timeline,
                             std::unordered_map<std::uint64_t, std::string> paths,
                             oma::SampleRate rate, int channels)
    : timeline_(std::move(timeline)), paths_(std::move(paths)), rate_(rate), channels_(channels) {
    // The sequence timebase holds whole samples (Timeline::default_timebase).
    ticks_per_sample_ =
        std::max<std::int64_t>(1, timeline_.to_ticks(rate_.sample_to_time(1)).value_or(1));
}

oma::Result<void> TimelineAudio::render(std::span<float> out, std::int64_t first) {
    ++pass_;
    std::ranges::fill(out, 0.0F);
    const auto frames = static_cast<std::int64_t>(out.size() / static_cast<std::size_t>(channels_));
    oma::Result<void> result;
    for (const tl::Track& track : timeline_.tracks()) {
        if (track.muted || track.kind == tl::TrackKind::Caption) {
            continue;
        }
        for (std::size_t i = 0; i < track.clips.size(); ++i) {
            const tl::Clip& clip = track.clips[i];
            const tl::MediaInfo* media = timeline_.find_media(clip.media);
            if (media == nullptr || !media->has_audio || clip.audio.muted || clip.audio_detached) {
                continue;
            }
            const Span span = span_of(track, i);
            if (span.end + span.tail <= first || span.first - span.lead >= first + frames) {
                continue;
            }
            if (auto r = mix_clip(clip, span, out, first); !r && result) {
                result = r; // report the first failure; the rest of the mix still plays
            }
        }
    }
    std::erase_if(streams_, [&](const auto& entry) { return entry.second.used_in != pass_; });
    return result;
}

TimelineAudio::Span TimelineAudio::span_of(const tl::Track& track, std::size_t index) const {
    const tl::Clip& clip = track.clips[index];
    // The clip owns the samples whose start lies inside [start, end).
    Span span{.first = ceil_div(clip.start_ticks(), ticks_per_sample_),
              .end = ceil_div(clip.end_ticks(), ticks_per_sample_),
              .lead = 0,
              .tail = 0};
    // A transition crossfades over the window around the cut, the same span as the picture.
    if (const auto in = tl::transition_window(timeline_, track, index)) {
        span.lead = span.first - ceil_div(in->cut - in->half, ticks_per_sample_);
    }
    if (const auto out = tl::transition_window(timeline_, track, index + 1)) {
        span.tail = ceil_div(out->cut + out->half, ticks_per_sample_) - span.end;
    }
    return span;
}

oma::Result<TimelineAudio::Stream*> TimelineAudio::stream(const tl::Clip& clip,
                                                          std::int64_t media_sample) {
    if (const auto own = streams_.find(clip.id.value()); own != streams_.end()) {
        own->second.used_in = pass_;
        return &own->second;
    }
    const auto continued = std::ranges::find_if(streams_, [&](const auto& entry) {
        const Stream& s = entry.second;
        return s.finished && !s.failed && s.media == clip.media.value() &&
               s.noise == clip.audio.noise && s.next_sample == media_sample;
    });
    if (continued != streams_.end()) {
        auto node = streams_.extract(continued);
        node.key() = clip.id.value();
        Stream& s = streams_.insert(std::move(node)).position->second;
        s.finished = false;
        s.used_in = pass_;
        // Same settings or not, the effect state carries over: the samples are contiguous.
        configure_effects(s.effects, clip.audio, rate_.hz());
        return &s;
    }
    Stream& s = streams_[clip.id.value()];
    s.media = clip.media.value();
    s.noise = clip.audio.noise;
    configure_effects(s.effects, clip.audio, rate_.hz());
    s.used_in = pass_;
    s.next_sample = -1; // forces the first seek
    const auto path = paths_.find(clip.media.value());
    if (path == paths_.end()) {
        s.failed = true;
        return oma::make_error(oma::ErrorCode::InvalidArgument, oma::Category::Audio,
                               "clip media has no file");
    }
    oma::media::AudioDecoderOptions options;
    options.sample_rate = rate_;
    options.channels = channels_;
    options.denoise = clip.audio.noise.amount;
    options.noise_floor_db = clip.audio.noise.floor_db;
    auto decoder = oma::media::AudioDecoder::open(std::filesystem::path(path->second), options);
    if (!decoder) {
        s.failed = true;
        return std::unexpected(decoder.error());
    }
    s.decoder = std::move(*decoder);
    return &s;
}

oma::Result<void> TimelineAudio::mix_clip(const tl::Clip& clip, const Span& span,
                                          std::span<float> out, std::int64_t first) {
    if (clip.time_map.speed() != oma::Rational::literal(1, 1)) {
        return {}; // needs time-stretching (v0.2 speed work)
    }
    const auto frames = static_cast<std::int64_t>(out.size() / static_cast<std::size_t>(channels_));
    const std::int64_t a = span.first;
    const std::int64_t b = span.end;
    // Transitions play the media past the clip's edges; the window was checked to have it.
    const std::int64_t from = std::max(a - span.lead, first);
    const std::int64_t to = std::min(b + span.tail, first + frames);
    const std::int64_t media_sample =
        sample_at(clip.source_in, (from * ticks_per_sample_) - clip.start_ticks(),
                  timeline_.timebase(), rate_.hz());
    auto opened = stream(clip, media_sample);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    Stream& s = **opened;
    if (s.failed) {
        return {};
    }
    if (s.next_sample != media_sample) {
        // A jump (first block, a seek, or a cut back into this media): reposition exactly.
        auto t = oma::RationalTime::make(media_sample, rate_.timebase());
        if (!t) {
            return std::unexpected(t.error());
        }
        if (auto r = s.decoder->seek(*t); !r) {
            s.failed = true;
            return r;
        }
        s.buffer.reset();
        s.offset = 0;
        s.ended = false;
        s.next_sample = media_sample;
        s.effects.reset();
    }
    const oma::audio::ClipGain gain{
        .gain = clip.audio.gain,
        .length = b - a,
        .fade_in = rate_.time_to_sample(clip.audio.fade_in, oma::Rounding::Floor).value_or(0),
        .fade_out = rate_.time_to_sample(clip.audio.fade_out, oma::Rounding::Floor).value_or(0),
        .lead = span.lead,
        .tail = span.tail};
    std::int64_t pos = from;
    while (pos < to) {
        if (!s.buffer || s.offset >= s.buffer->frames) {
            if (s.ended) {
                break;
            }
            auto next = s.decoder->next();
            if (!next) {
                s.failed = true;
                return std::unexpected(next.error());
            }
            if (!*next) {
                s.ended = true;
                break;
            }
            s.buffer = std::move(*next);
            s.offset = 0;
        }
        const std::int64_t n = std::min(to - pos, s.buffer->frames - s.offset);
        const auto at = static_cast<std::size_t>((pos - first) * channels_);
        const auto input = std::span<const float>(s.buffer->samples);
        if (s.effects.active()) {
            // Process a copy: the buffer may be mixed again after a seek back into it.
            const int ch = s.buffer->channels;
            scratch_.resize(static_cast<std::size_t>(n) * static_cast<std::size_t>(ch));
            for (int c = 0; c < ch; ++c) {
                const auto src =
                    (static_cast<std::size_t>(c) * static_cast<std::size_t>(s.buffer->frames)) +
                    static_cast<std::size_t>(s.offset);
                std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(src), n,
                            scratch_.begin() + (static_cast<std::ptrdiff_t>(c) * n));
            }
            s.effects.process(scratch_, ch, n, n);
            oma::audio::mix_planar(out.subspan(at), channels_, scratch_, ch, n, n, gain, pos - a);
        } else {
            oma::audio::mix_planar(out.subspan(at), channels_,
                                   input.subspan(static_cast<std::size_t>(s.offset)),
                                   s.buffer->channels, s.buffer->frames, n, gain, pos - a);
        }
        s.offset += n;
        s.next_sample += n;
        pos += n;
    }
    s.finished = to == b + span.tail;
    return {};
}

} // namespace oma::playback
