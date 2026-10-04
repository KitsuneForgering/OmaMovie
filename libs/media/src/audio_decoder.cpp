#include "oma/media/audio_decoder.hpp"

#include "ffmpeg.hpp"
#include "stream_info.hpp"

#include "oma/base/log.hpp"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <utility>

namespace oma::media {

std::span<const float> AudioBuffer::channel(int c) const noexcept {
    if (c < 0 || c >= channels) {
        return {};
    }
    const auto n = static_cast<std::size_t>(frames);
    return {samples.data() + (static_cast<std::size_t>(c) * n), n};
}

namespace {

constexpr int kDrainMargin = 256;
// Decoded before a seek target when denoising, so the noise estimate has settled at the target.
constexpr std::int64_t kDenoisePrerollMs = 500;
// afftdn's reduction at full amount, in dB (its range is 0.01 to 97).
constexpr double kMaxReductionDb = 30.0;
// How far above the measured noise level afftdn's noise floor is set.
constexpr double kFloorMarginDb = 3.0;

struct GraphFreer {
    void operator()(AVFilterGraph* g) const noexcept { avfilter_graph_free(&g); }
};
using GraphPtr = std::unique_ptr<AVFilterGraph, GraphFreer>;

struct InOutFreer {
    void operator()(AVFilterInOut* p) const noexcept { avfilter_inout_free(&p); }
};
using InOutPtr = std::unique_ptr<AVFilterInOut, InOutFreer>;

struct ChannelLayout {
    AVChannelLayout layout{};
    ChannelLayout() = default;
    ChannelLayout(const ChannelLayout&) = delete;
    ChannelLayout& operator=(const ChannelLayout&) = delete;
    ChannelLayout(ChannelLayout&&) = delete;
    ChannelLayout& operator=(ChannelLayout&&) = delete;
    ~ChannelLayout() { av_channel_layout_uninit(&layout); }
};

} // namespace

struct AudioDecoder::Impl {
    std::filesystem::path file;
    ff::FormatPtr fmt;
    AVStream* st = nullptr;
    StreamInfo info;
    ff::CodecPtr dec;
    ff::SwrPtr swr;
    ff::PacketPtr pkt;
    ff::FramePtr frame;

    SampleRate out_rate = SampleRate::make(48000).value();
    ChannelLayout out_layout;
    ChannelLayout in_source; // the frames' layout as decoded, to detect changes
    ChannelLayout in_layout; // the layout swr was configured for (unspecified order resolved)
    int in_rate = 0;
    AVSampleFormat in_format = AV_SAMPLE_FMT_NONE;

    bool flushed = false;                    // swr drained after the decoder ended
    std::optional<std::int64_t> next_sample; // output position of the next sample
    std::optional<std::int64_t> trim_before; // after a seek: drop output before this sample

    // Noise reduction (options.denoise > 0): decoded buffers go through a filter graph built on
    // first use and rebuilt after every seek.
    float denoise = 0.0F;
    float noise_floor_db = -50.0F;
    GraphPtr graph;
    AVFilterContext* source = nullptr;
    AVFilterContext* sink = nullptr;
    bool graph_flushed = false;               // the decoder's end was sent into the graph
    std::optional<std::int64_t> deliver_from; // after a seek: drop filtered output before this

    [[nodiscard]] Result<void> configure_resampler(const AVFrame& f);
    [[nodiscard]] Result<std::optional<AudioBuffer>> convert(const AVFrame* f);
    [[nodiscard]] Result<std::optional<AudioBuffer>> next_decoded();
    [[nodiscard]] Result<void> build_graph();
    [[nodiscard]] Result<void> push(const AudioBuffer& buffer);
    [[nodiscard]] Result<std::optional<AudioBuffer>> next_denoised();
};

Result<void> AudioDecoder::Impl::configure_resampler(const AVFrame& f) {
    const bool same = swr && f.sample_rate == in_rate && f.format == in_format &&
                      av_channel_layout_compare(&f.ch_layout, &in_source.layout) == 0;
    if (same) {
        return {};
    }
    av_channel_layout_uninit(&in_source.layout);
    av_channel_layout_uninit(&in_layout.layout);
    if (const int err = av_channel_layout_copy(&in_source.layout, &f.ch_layout); err < 0) {
        return std::unexpected(ff::av_error(err, Category::Audio, "invalid channel layout"));
    }
    if (f.ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        av_channel_layout_default(&in_layout.layout, f.ch_layout.nb_channels);
    } else if (const int err = av_channel_layout_copy(&in_layout.layout, &f.ch_layout); err < 0) {
        return std::unexpected(ff::av_error(err, Category::Audio, "invalid channel layout"));
    }
    SwrContext* raw = nullptr;
    int err = swr_alloc_set_opts2(&raw, &out_layout.layout, AV_SAMPLE_FMT_FLTP, out_rate.hz(),
                                  &in_layout.layout, static_cast<AVSampleFormat>(f.format),
                                  f.sample_rate, 0, nullptr);
    ff::SwrPtr ctx(raw);
    if (err >= 0) {
        err = swr_init(ctx.get());
    }
    if (err < 0) {
        return std::unexpected(ff::av_error(err, Category::Audio, "cannot configure resampling"));
    }
    swr = std::move(ctx);
    in_rate = f.sample_rate;
    in_format = static_cast<AVSampleFormat>(f.format);
    return {};
}

// Converts one decoded frame (or drains the resampler when f is null) into a buffer.
Result<std::optional<AudioBuffer>> AudioDecoder::Impl::convert(const AVFrame* f) {
    if (f != nullptr) {
        if (auto r = configure_resampler(*f); !r) {
            return std::unexpected(r.error());
        }
        if (!next_sample) {
            const auto pts = ff::to_time(f->best_effort_timestamp, info.timebase);
            const auto start =
                pts ? out_rate.time_to_sample(*pts, Rounding::Nearest) : Result<std::int64_t>(0);
            if (!start) {
                return std::unexpected(start.error());
            }
            next_sample = *start;
        }
    }
    if (!swr) {
        return std::optional<AudioBuffer>();
    }
    const int in_samples = f != nullptr ? f->nb_samples : 0;
    // When draining, swr_get_out_samples() underestimates what the filter still holds.
    const int capacity =
        f != nullptr ? swr_get_out_samples(swr.get(), in_samples)
                     : static_cast<int>(swr_get_delay(swr.get(), out_rate.hz())) + kDrainMargin;
    if (capacity <= 0) {
        return std::optional<AudioBuffer>();
    }
    const int channels = out_layout.layout.nb_channels;
    std::vector<float> samples(static_cast<std::size_t>(capacity) *
                               static_cast<std::size_t>(channels));
    std::array<std::uint8_t*, AV_NUM_DATA_POINTERS> out{};
    for (int c = 0; c < channels && c < AV_NUM_DATA_POINTERS; ++c) {
        out[static_cast<std::size_t>(c)] = reinterpret_cast<std::uint8_t*>(
            samples.data() + (static_cast<std::size_t>(c) * static_cast<std::size_t>(capacity)));
    }
    const int n = swr_convert(
        swr.get(), out.data(), capacity,
        f != nullptr ? const_cast<const std::uint8_t**>(f->extended_data) : nullptr, in_samples);
    if (n < 0) {
        return std::unexpected(ff::av_error(n, Category::Audio, "resampling failed"));
    }

    std::int64_t start = next_sample.value_or(0);
    const std::int64_t end = start + n;
    next_sample = end;
    int first = 0;
    if (const std::optional<std::int64_t> trim = trim_before) {
        if (end <= *trim) {
            return std::optional<AudioBuffer>(); // entirely before the seek target
        }
        first = static_cast<int>(std::max<std::int64_t>(0, *trim - start));
        start += first;
        trim_before.reset();
    }
    const int kept = n - first;
    if (kept <= 0) {
        return std::optional<AudioBuffer>();
    }
    // Compact the planes: each was laid out with `capacity` samples.
    for (int c = 0; c < channels; ++c) {
        const auto src = (static_cast<std::size_t>(c) * static_cast<std::size_t>(capacity)) +
                         static_cast<std::size_t>(first);
        const auto dst = static_cast<std::size_t>(c) * static_cast<std::size_t>(kept);
        std::memmove(samples.data() + dst, samples.data() + src,
                     static_cast<std::size_t>(kept) * sizeof(float));
    }
    samples.resize(static_cast<std::size_t>(kept) * static_cast<std::size_t>(channels));
    return std::optional<AudioBuffer>(AudioBuffer{.pts = out_rate.sample_to_time(start),
                                                  .sample_rate = out_rate,
                                                  .channels = channels,
                                                  .frames = kept,
                                                  .samples = std::move(samples)});
}

AudioDecoder::AudioDecoder(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
AudioDecoder::~AudioDecoder() = default;

Result<std::unique_ptr<AudioDecoder>> AudioDecoder::open(const std::filesystem::path& path,
                                                         const AudioDecoderOptions& options) {
    auto impl = std::make_unique<Impl>();
    impl->file = path;
    auto fmt = ff::open_input(path);
    if (!fmt) {
        return std::unexpected(fmt.error());
    }
    impl->fmt = std::move(*fmt);
    const int index = options.stream.value_or(
        av_find_best_stream(impl->fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0));
    if (index < 0 || std::cmp_greater_equal(index, impl->fmt->nb_streams) ||
        impl->fmt->streams[index]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        return make_error(ErrorCode::InvalidArgument, Category::Audio, "no such audio stream",
                          path.string());
    }
    impl->st = impl->fmt->streams[index];
    for (unsigned i = 0; i < impl->fmt->nb_streams; ++i) {
        impl->fmt->streams[i]->discard =
            std::cmp_equal(i, index) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }
    const auto tb = ff::to_timebase(impl->st->time_base);
    if (!tb) {
        return make_error(ErrorCode::InvalidData, Category::Audio, "stream has no timebase",
                          path.string());
    }
    const AVCodecParameters& par = *impl->st->codecpar;
    impl->info = describe_stream(*impl->st, impl->fmt->iformat->name);

    const AVCodec* codec = avcodec_find_decoder(par.codec_id);
    if (codec == nullptr) {
        return make_error(ErrorCode::Unsupported, Category::Audio, "no decoder", impl->info.codec);
    }
    auto ctx = ff::alloc_codec(*impl->st, *codec);
    if (!ctx) {
        return std::unexpected(ctx.error());
    }
    if (const int err = avcodec_open2(ctx->get(), codec, nullptr); err < 0) {
        return std::unexpected(
            ff::av_error(err, Category::Audio, "cannot open decoder", codec->name));
    }
    impl->dec = std::move(*ctx);

    if (options.sample_rate) {
        impl->out_rate = *options.sample_rate;
    } else if (auto rate = SampleRate::make(impl->dec->sample_rate)) {
        impl->out_rate = *rate;
    }
    const int channels = options.channels.value_or(impl->dec->ch_layout.nb_channels);
    if (channels <= 0 || channels > AV_NUM_DATA_POINTERS) {
        return make_error(ErrorCode::Unsupported, Category::Audio, "unsupported channel count",
                          std::to_string(channels));
    }
    if (!options.channels && impl->dec->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC) {
        if (const int err = av_channel_layout_copy(&impl->out_layout.layout, &impl->dec->ch_layout);
            err < 0) {
            return std::unexpected(ff::av_error(err, Category::Audio, "invalid channel layout"));
        }
    } else {
        av_channel_layout_default(&impl->out_layout.layout, channels);
    }

    if (!std::isfinite(options.denoise) || options.denoise < 0.0F || options.denoise > 1.0F) {
        return make_error(ErrorCode::InvalidArgument, Category::Audio,
                          "denoise amount outside [0, 1]");
    }
    if (!std::isfinite(options.noise_floor_db)) {
        return make_error(ErrorCode::InvalidArgument, Category::Audio, "invalid noise floor");
    }
    impl->denoise = options.denoise;
    impl->noise_floor_db = options.noise_floor_db;

    impl->pkt.reset(av_packet_alloc());
    impl->frame.reset(av_frame_alloc());
    if (!impl->pkt || !impl->frame) {
        return make_error(ErrorCode::Internal, Category::Audio, "cannot allocate decode buffers");
    }
    return std::unique_ptr<AudioDecoder>(new AudioDecoder(std::move(impl)));
}

const StreamInfo& AudioDecoder::stream() const noexcept {
    return impl_->info;
}
SampleRate AudioDecoder::sample_rate() const noexcept {
    return impl_->out_rate;
}
int AudioDecoder::channels() const noexcept {
    return impl_->out_layout.layout.nb_channels;
}

Result<std::optional<AudioBuffer>> AudioDecoder::Impl::next_decoded() {
    Impl& d = *this;
    for (;;) {
        const int r = avcodec_receive_frame(d.dec.get(), d.frame.get());
        if (r == 0) {
            auto buffer = d.convert(d.frame.get());
            av_frame_unref(d.frame.get());
            if (!buffer || *buffer) {
                return buffer;
            }
            continue;
        }
        if (r == AVERROR_EOF) {
            if (d.flushed) {
                return std::optional<AudioBuffer>();
            }
            // Samples still inside the resampler, possibly over several calls.
            auto tail = d.convert(nullptr);
            if (!tail || *tail) {
                return tail;
            }
            d.flushed = true;
            return std::optional<AudioBuffer>();
        }
        if (r != AVERROR(EAGAIN)) {
            return std::unexpected(
                ff::av_error(r, Category::Audio, "decode failed", d.file.string()));
        }
        const int rd = av_read_frame(d.fmt.get(), d.pkt.get());
        if (rd < 0) {
            if (rd != AVERROR_EOF) {
                log_warn(Category::Audio, "{}: read error, ending the stream: {}",
                         d.file.filename().string(), ff::error_string(rd));
            }
            avcodec_send_packet(d.dec.get(), nullptr);
            continue;
        }
        if (d.pkt->stream_index != d.st->index) {
            av_packet_unref(d.pkt.get());
            continue;
        }
        const int sr = avcodec_send_packet(d.dec.get(), d.pkt.get());
        av_packet_unref(d.pkt.get());
        if (sr < 0 && sr != AVERROR(EAGAIN) && sr != AVERROR_INVALIDDATA) {
            return std::unexpected(
                ff::av_error(sr, Category::Audio, "decode failed", d.file.string()));
        }
    }
}

Result<void> AudioDecoder::Impl::build_graph() {
    graph.reset(avfilter_graph_alloc());
    if (!graph) {
        return make_error(ErrorCode::Internal, Category::Audio, "cannot allocate a filter graph");
    }
    std::array<char, 128> layout{};
    av_channel_layout_describe(&out_layout.layout, layout.data(), layout.size());
    const int rate = out_rate.hz();
    const std::string args =
        std::format("time_base=1/{}:sample_rate={}:sample_fmt=fltp:channel_layout={}", rate, rate,
                    layout.data());
    int err = avfilter_graph_create_filter(&source, avfilter_get_by_name("abuffer"), "in",
                                           args.c_str(), nullptr, graph.get());
    if (err >= 0) {
        err = avfilter_graph_create_filter(&sink, avfilter_get_by_name("abuffersink"), "out",
                                           nullptr, nullptr, graph.get());
    }
    if (err < 0) {
        return std::unexpected(ff::av_error(err, Category::Audio, "cannot create the denoiser"));
    }
    InOutPtr outputs(avfilter_inout_alloc());
    InOutPtr inputs(avfilter_inout_alloc());
    if (!outputs || !inputs) {
        return make_error(ErrorCode::Internal, Category::Audio, "cannot allocate filter pads");
    }
    outputs->name = av_strdup("in");
    outputs->filter_ctx = source;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink;
    // afftdn's floor sits a little above the measured noise: below it, it barely acts (measured
    // on tests/fixtures noisy_tone.wav: floor at the noise level removes ~12 dB, 3 dB above it
    // ~28 dB, with the tone unchanged). Its own noise tracking (tn) is left off: it cancelled the
    // reduction in the same measurement. aformat keeps the planar float layout the mixer expects.
    const double reduction = std::max(0.01, static_cast<double>(denoise) * kMaxReductionDb);
    const double floor_db =
        std::clamp(static_cast<double>(noise_floor_db) + kFloorMarginDb, -80.0, -20.0);
    const std::string chain =
        std::format("afftdn=nr={:.2f}:nf={:.1f},aformat=sample_fmts=fltp", reduction, floor_db);
    AVFilterInOut* in = inputs.release();
    AVFilterInOut* out = outputs.release();
    err = avfilter_graph_parse_ptr(graph.get(), chain.c_str(), &in, &out, nullptr);
    avfilter_inout_free(&in);
    avfilter_inout_free(&out);
    if (err >= 0) {
        err = avfilter_graph_config(graph.get(), nullptr);
    }
    if (err < 0) {
        graph.reset();
        return std::unexpected(
            ff::av_error(err, Category::Audio, "cannot configure the denoiser", chain));
    }
    graph_flushed = false;
    return {};
}

Result<void> AudioDecoder::Impl::push(const AudioBuffer& buffer) {
    ff::FramePtr f(av_frame_alloc());
    if (!f) {
        return make_error(ErrorCode::Internal, Category::Audio, "cannot allocate a frame");
    }
    f->format = AV_SAMPLE_FMT_FLTP;
    f->sample_rate = out_rate.hz();
    f->nb_samples = static_cast<int>(buffer.frames);
    f->pts = out_rate.time_to_sample(buffer.pts, Rounding::Nearest).value_or(0);
    int err = av_channel_layout_copy(&f->ch_layout, &out_layout.layout);
    if (err >= 0) {
        err = av_frame_get_buffer(f.get(), 0);
    }
    if (err < 0) {
        return std::unexpected(ff::av_error(err, Category::Audio, "cannot allocate a frame"));
    }
    for (int c = 0; c < buffer.channels; ++c) {
        const auto plane = buffer.channel(c);
        std::memcpy(f->extended_data[c], plane.data(), plane.size_bytes());
    }
    if (err = av_buffersrc_add_frame(source, f.get()); err < 0) {
        return std::unexpected(ff::av_error(err, Category::Audio, "denoising failed"));
    }
    return {};
}

Result<std::optional<AudioBuffer>> AudioDecoder::Impl::next_denoised() {
    if (!graph) {
        if (auto r = build_graph(); !r) {
            return std::unexpected(r.error());
        }
    }
    for (;;) {
        const int r = av_buffersink_get_frame(sink, frame.get());
        if (r == AVERROR_EOF) {
            return std::optional<AudioBuffer>();
        }
        if (r == AVERROR(EAGAIN)) {
            if (graph_flushed) {
                return std::optional<AudioBuffer>();
            }
            auto decoded = next_decoded();
            if (!decoded) {
                return decoded;
            }
            if (!*decoded) {
                graph_flushed = true;
                if (const int err = av_buffersrc_add_frame(source, nullptr); err < 0) {
                    return std::unexpected(ff::av_error(err, Category::Audio, "denoising failed"));
                }
            } else if (auto pushed = push(**decoded); !pushed) {
                return std::unexpected(pushed.error());
            }
            continue;
        }
        if (r < 0) {
            return std::unexpected(ff::av_error(r, Category::Audio, "denoising failed"));
        }
        // The graph keeps the source's 1/rate timebase: pts counts samples.
        std::int64_t start = frame->pts;
        int first = 0;
        const int n = frame->nb_samples;
        if (deliver_from) {
            if (start + n <= *deliver_from) {
                av_frame_unref(frame.get());
                continue; // the settling stretch before the seek target
            }
            first = static_cast<int>(std::max<std::int64_t>(0, *deliver_from - start));
            start += first;
            deliver_from.reset();
        }
        const int kept = n - first;
        const int channels = frame->ch_layout.nb_channels;
        std::vector<float> samples(static_cast<std::size_t>(kept) *
                                   static_cast<std::size_t>(channels));
        for (int c = 0; c < channels; ++c) {
            const auto* plane = reinterpret_cast<const float*>(frame->extended_data[c]);
            std::memcpy(samples.data() +
                            (static_cast<std::size_t>(c) * static_cast<std::size_t>(kept)),
                        plane + first, static_cast<std::size_t>(kept) * sizeof(float));
        }
        av_frame_unref(frame.get());
        if (kept <= 0) {
            continue;
        }
        return std::optional<AudioBuffer>(AudioBuffer{.pts = out_rate.sample_to_time(start),
                                                      .sample_rate = out_rate,
                                                      .channels = channels,
                                                      .frames = kept,
                                                      .samples = std::move(samples)});
    }
}

Result<std::optional<AudioBuffer>> AudioDecoder::next() {
    return impl_->denoise > 0.0F ? impl_->next_denoised() : impl_->next_decoded();
}

Result<void> AudioDecoder::seek(const RationalTime& t) {
    Impl& d = *impl_;
    auto target = d.out_rate.time_to_sample(t, Rounding::Floor);
    if (!target) {
        return std::unexpected(target.error());
    }
    std::int64_t decode_from = *target;
    if (d.denoise > 0.0F) {
        // Decode a little earlier and discard it after filtering: the denoiser adapts to the
        // material instead of starting cold exactly at the target.
        decode_from =
            std::max<std::int64_t>(0, *target - (kDenoisePrerollMs * d.out_rate.hz() / 1000));
        d.graph.reset();
        d.deliver_from = *target;
    }
    const RationalTime from = d.out_rate.sample_to_time(decode_from);
    auto ticks = ff::to_ticks(from, d.info.timebase, Rounding::Floor);
    if (!ticks) {
        return std::unexpected(ticks.error());
    }
    if (const int err = av_seek_frame(d.fmt.get(), d.st->index, *ticks, AVSEEK_FLAG_BACKWARD);
        err < 0) {
        if (const int err2 =
                av_seek_frame(d.fmt.get(), d.st->index, INT64_MIN, AVSEEK_FLAG_BACKWARD);
            err2 < 0) {
            return std::unexpected(
                ff::av_error(err, Category::Audio, "seek failed", d.file.string()));
        }
    }
    avcodec_flush_buffers(d.dec.get());
    d.swr.reset(); // drop samples buffered for the old position
    d.flushed = false;
    d.next_sample.reset();
    d.trim_before = decode_from;
    return {};
}

} // namespace oma::media
