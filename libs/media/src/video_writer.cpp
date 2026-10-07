#include "oma/media/video_writer.hpp"

#include "ffmpeg.hpp"

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oma::media {

namespace {

struct OutputCloser {
    void operator()(AVFormatContext* p) const noexcept {
        if (p != nullptr) {
            if (p->pb != nullptr)
                avio_closep(&p->pb);
            avformat_free_context(p);
        }
    }
};
struct SwsCloser {
    void operator()(SwsContext* p) const noexcept { sws_freeContext(p); }
};
struct FifoCloser {
    void operator()(AVAudioFifo* p) const noexcept { av_audio_fifo_free(p); }
};

Error io_error(const char* what, const std::filesystem::path& path, int err) {
    return {ErrorCode::IoError, Category::Encode,
            std::format("{}: {}", what, ff::error_string(AVERROR(err))), path.string()};
}

} // namespace

struct VideoWriter::Impl {
    std::filesystem::path file;
    std::string temporary;
    std::unique_ptr<AVFormatContext, OutputCloser> format;
    ff::CodecPtr codec;
    ff::FramePtr frame;
    ff::PacketPtr packet;
    std::unique_ptr<SwsContext, SwsCloser> scale;
    std::unique_ptr<SwsContext, SwsCloser> scale_rgba; // made on the first write_rgba
    AVPixelFormat sw_format = AV_PIX_FMT_YUV420P;      // what swscale writes (NV12 for VA-API)
    // VA-API: `frame` is the system-memory NV12 picture, uploaded into `hw_frame` each time.
    ff::BufferPtr hw_device;
    ff::BufferPtr hw_frames;
    ff::FramePtr hw_frame;
    AVStream* stream = nullptr;
    std::int64_t next_pts = 0;
    bool finished = false;
    // Audio (optional): planar float frames of the encoder's frame size, from a FIFO.
    ff::CodecPtr audio;
    ff::FramePtr audio_frame;
    std::unique_ptr<AVAudioFifo, FifoCloser> fifo;
    AVStream* audio_stream = nullptr;
    std::int64_t audio_pts = 0;
    std::vector<float> planar; // reused deinterleave buffer

    ~Impl() {
        format.reset();
        if (!finished && !temporary.empty())
            ::unlink(temporary.c_str());
    }

    Result<void> drain() const { return drain(codec.get(), stream); }
    Result<void> encode(SwsContext* from, std::span<const std::uint8_t> pixels, int stride,
                        int bpp);
    Result<void> drain(AVCodecContext* from, AVStream* to) const;
    // Encodes whole frames from the FIFO; with `flush`, also the remainder, padded with silence.
    Result<void> encode_audio(bool flush);
};

Result<void> VideoWriter::Impl::drain(AVCodecContext* from, AVStream* to) const {
    for (;;) {
        const int err = avcodec_receive_packet(from, packet.get());
        if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
            return {};
        if (err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "cannot encode"));
        // libx264 may leave duration unset; MP4 needs it to retain the final frame.
        if (packet->duration <= 0)
            packet->duration = 1;
        av_packet_rescale_ts(packet.get(), from->time_base, to->time_base);
        packet->stream_index = to->index;
        const int written = av_interleaved_write_frame(format.get(), packet.get());
        av_packet_unref(packet.get());
        if (written < 0) {
            return std::unexpected(
                ff::av_error(written, Category::Encode, "cannot write packet", file.string()));
        }
    }
}

Result<void> VideoWriter::Impl::encode_audio(bool flush) {
    const int size = audio->frame_size;
    while (av_audio_fifo_size(fifo.get()) >= size ||
           (flush && av_audio_fifo_size(fifo.get()) > 0)) {
        if (const int err = av_frame_make_writable(audio_frame.get()); err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "cannot reuse audio frame"));
        const int got =
            av_audio_fifo_read(fifo.get(), reinterpret_cast<void**>(audio_frame->data), size);
        if (got < 0)
            return std::unexpected(ff::av_error(got, Category::Encode, "cannot read audio FIFO"));
        for (int ch = 0; ch < audio->ch_layout.nb_channels; ++ch) { // pad the last frame
            auto* plane = reinterpret_cast<float*>(audio_frame->data[ch]);
            std::fill(plane + got, plane + size, 0.0F);
        }
        audio_frame->pts = audio_pts;
        audio_pts += size;
        if (const int err = avcodec_send_frame(audio.get(), audio_frame.get()); err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "cannot submit audio"));
        if (auto drained = drain(audio.get(), audio_stream); !drained)
            return drained;
    }
    return {};
}

VideoWriter::VideoWriter(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
VideoWriter::~VideoWriter() = default;

Result<std::unique_ptr<VideoWriter>> VideoWriter::create(const std::filesystem::path& file,
                                                         int width, int height, FrameRate rate,
                                                         const VideoWriterOptions& options) {
    const std::optional<AudioTrack>& audio = options.audio;
    const bool vaapi = options.encoder == VideoEncoder::VaApi;
    if (file.empty() || width < 16 || height < 16 || width > 16384 || height > 16384 ||
        (width & 1) != 0 || (height & 1) != 0 || rate.fps().num() > INT_MAX ||
        rate.fps().den() > INT_MAX ||
        (audio && (audio->channels < 1 || audio->channels > 2 || audio->rate.hz() < 8000 ||
                   audio->rate.hz() > 192000))) {
        return make_error(ErrorCode::InvalidArgument, Category::Encode,
                          "invalid video output settings");
    }
    ff::ensure_initialized();
    const AVCodec* encoder = avcodec_find_encoder_by_name(vaapi ? "h264_vaapi" : "libx264");
    if (encoder == nullptr) {
        return make_error(ErrorCode::Unsupported, Category::Encode,
                          vaapi ? "the VA-API H.264 encoder is unavailable"
                                : "H.264 software encoder is unavailable");
    }
    auto impl = std::make_unique<Impl>();
    impl->file = file;
    impl->temporary = file.string() + ".tmp.XXXXXX";
    const int fd = ::mkstemp(impl->temporary.data());
    if (fd < 0)
        return std::unexpected(io_error("cannot create temporary output", file, errno));
    // mkstemp creates 0600; the finished file is an ordinary movie (as project saves do).
    if (::fchmod(fd, 0644) != 0) {
        const int err = errno;
        ::close(fd);
        return std::unexpected(io_error("cannot set output permissions", file, err));
    }
    if (::close(fd) != 0)
        return std::unexpected(io_error("cannot close temporary output", file, errno));

    AVFormatContext* raw = nullptr;
    int err = avformat_alloc_output_context2(&raw, nullptr, "mp4", impl->temporary.c_str());
    impl->format.reset(raw);
    if (err < 0 || raw == nullptr) {
        return std::unexpected(
            ff::av_error(err, Category::Encode, "cannot create MP4 output", file.string()));
    }
    impl->stream = avformat_new_stream(raw, nullptr);
    impl->codec.reset(avcodec_alloc_context3(encoder));
    impl->frame.reset(av_frame_alloc());
    impl->packet.reset(av_packet_alloc());
    if (impl->stream == nullptr || !impl->codec || !impl->frame || !impl->packet) {
        return make_error(ErrorCode::Internal, Category::Encode,
                          "cannot allocate video encoder resources");
    }
    auto& c = *impl->codec;
    c.width = width;
    c.height = height;
    c.pix_fmt = AV_PIX_FMT_YUV420P;
    if (vaapi) {
        AVBufferRef* device = nullptr;
        const std::string node = options.render_node.string();
        int made = av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VAAPI,
                                          node.empty() ? nullptr : node.c_str(), nullptr, 0);
        impl->hw_device.reset(device);
        if (made < 0)
            return std::unexpected(
                ff::av_error(made, Category::Encode, "cannot open VA-API", node));
        impl->hw_frames.reset(av_hwframe_ctx_alloc(impl->hw_device.get()));
        impl->hw_frame.reset(av_frame_alloc());
        if (!impl->hw_frames || !impl->hw_frame)
            return make_error(ErrorCode::Internal, Category::Encode,
                              "cannot allocate VA-API frames");
        auto* frames = reinterpret_cast<AVHWFramesContext*>(impl->hw_frames->data);
        frames->format = AV_PIX_FMT_VAAPI;
        frames->sw_format = AV_PIX_FMT_NV12;
        frames->width = width;
        frames->height = height;
        frames->initial_pool_size = 16;
        if (made = av_hwframe_ctx_init(impl->hw_frames.get()); made < 0)
            return std::unexpected(
                ff::av_error(made, Category::Encode, "cannot create VA-API surfaces"));
        c.hw_frames_ctx = av_buffer_ref(impl->hw_frames.get());
        c.pix_fmt = AV_PIX_FMT_VAAPI;
        c.profile = AV_PROFILE_H264_HIGH;
        impl->sw_format = AV_PIX_FMT_NV12;
    }
    c.max_b_frames = 0;
    c.time_base = {.num = static_cast<int>(rate.fps().den()),
                   .den = static_cast<int>(rate.fps().num())};
    c.framerate = {.num = static_cast<int>(rate.fps().num()),
                   .den = static_cast<int>(rate.fps().den())};
    c.color_range = AVCOL_RANGE_MPEG;
    c.color_primaries = AVCOL_PRI_BT709;
    c.color_trc = AVCOL_TRC_BT709;
    c.colorspace = AVCOL_SPC_BT709;
    if ((raw->oformat->flags & AVFMT_GLOBALHEADER) != 0)
        c.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    err = avcodec_open2(&c, encoder, nullptr);
    if (err < 0) {
        return std::unexpected(ff::av_error(err, Category::Encode, "cannot open H.264 encoder"));
    }
    err = avcodec_parameters_from_context(impl->stream->codecpar, &c);
    if (err < 0) {
        return std::unexpected(ff::av_error(err, Category::Encode, "cannot set output stream"));
    }
    impl->stream->time_base = c.time_base;
    impl->frame->format = impl->sw_format;
    impl->frame->width = width;
    impl->frame->height = height;
    err = av_frame_get_buffer(impl->frame.get(), 32);
    if (err < 0) {
        return std::unexpected(ff::av_error(err, Category::Encode, "cannot allocate video frame"));
    }
    impl->scale.reset(sws_getContext(width, height, AV_PIX_FMT_RGB24, width, height,
                                     impl->sw_format, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!impl->scale) {
        return make_error(ErrorCode::Internal, Category::Encode, "cannot convert RGB to YUV");
    }
    if (sws_setColorspaceDetails(impl->scale.get(), sws_getCoefficients(SWS_CS_ITU709), 1,
                                 sws_getCoefficients(SWS_CS_ITU709), 0, 0, 1 << 16, 1 << 16) < 0) {
        return make_error(ErrorCode::Internal, Category::Encode, "cannot set BT.709 conversion");
    }
    if (audio) {
        const AVCodec* aac = avcodec_find_encoder(AV_CODEC_ID_AAC);
        impl->audio_stream = avformat_new_stream(raw, nullptr);
        impl->audio.reset(aac != nullptr ? avcodec_alloc_context3(aac) : nullptr);
        impl->audio_frame.reset(av_frame_alloc());
        if (aac == nullptr || impl->audio_stream == nullptr || !impl->audio || !impl->audio_frame) {
            return make_error(ErrorCode::Unsupported, Category::Encode,
                              "AAC encoder is unavailable");
        }
        auto& a = *impl->audio;
        a.sample_fmt = AV_SAMPLE_FMT_FLTP;
        a.sample_rate = static_cast<int>(audio->rate.hz());
        av_channel_layout_default(&a.ch_layout, audio->channels);
        a.bit_rate = 192000;
        a.time_base = {.num = 1, .den = a.sample_rate};
        if ((raw->oformat->flags & AVFMT_GLOBALHEADER) != 0)
            a.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (err = avcodec_open2(&a, aac, nullptr); err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "cannot open AAC encoder"));
        if (err = avcodec_parameters_from_context(impl->audio_stream->codecpar, &a); err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "cannot set audio stream"));
        impl->audio_stream->time_base = a.time_base;
        auto& f = *impl->audio_frame;
        f.format = a.sample_fmt;
        f.nb_samples = a.frame_size;
        f.sample_rate = a.sample_rate;
        err = av_channel_layout_copy(&f.ch_layout, &a.ch_layout);
        if (err >= 0)
            err = av_frame_get_buffer(&f, 0);
        if (err < 0) {
            return std::unexpected(
                ff::av_error(err, Category::Encode, "cannot allocate audio frame"));
        }
        impl->fifo.reset(av_audio_fifo_alloc(a.sample_fmt, audio->channels, a.frame_size * 4));
        if (!impl->fifo)
            return make_error(ErrorCode::Internal, Category::Encode, "cannot allocate audio FIFO");
    }
    err = avio_open(&raw->pb, impl->temporary.c_str(), AVIO_FLAG_WRITE);
    if (err < 0) {
        return std::unexpected(
            ff::av_error(err, Category::Encode, "cannot open temporary output", file.string()));
    }
    err = avformat_write_header(raw, nullptr);
    if (err < 0) {
        return std::unexpected(
            ff::av_error(err, Category::Encode, "cannot write MP4 header", file.string()));
    }
    return std::unique_ptr<VideoWriter>(new VideoWriter(std::move(impl)));
}

Result<void> VideoWriter::Impl::encode(SwsContext* from, std::span<const std::uint8_t> pixels,
                                       int stride, int bpp) {
    const int width = codec->width;
    const int height = codec->height;
    if (finished || stride < width * bpp ||
        pixels.size() < static_cast<std::size_t>(stride) * static_cast<std::size_t>(height) ||
        next_pts == std::numeric_limits<std::int64_t>::max()) {
        return make_error(ErrorCode::InvalidArgument, Category::Encode, "invalid video frame");
    }
    if (const int err = av_frame_make_writable(frame.get()); err < 0) {
        return std::unexpected(ff::av_error(err, Category::Encode, "cannot reuse video frame"));
    }
    const std::array<const std::uint8_t*, 4> data{pixels.data(), nullptr, nullptr, nullptr};
    const std::array<int, 4> linesize{stride, 0, 0, 0};
    const int rows =
        sws_scale(from, data.data(), linesize.data(), 0, height, frame->data, frame->linesize);
    if (rows != height) {
        return make_error(ErrorCode::Internal, Category::Encode, "cannot convert video frame");
    }
    AVFrame* send = frame.get();
    if (hw_frames) {
        // Upload into a VA surface from the encoder's pool (released when the encoder is done).
        av_frame_unref(hw_frame.get());
        if (const int err = av_hwframe_get_buffer(hw_frames.get(), hw_frame.get(), 0); err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "no free VA-API surface"));
        if (const int err = av_hwframe_transfer_data(hw_frame.get(), frame.get(), 0); err < 0)
            return std::unexpected(ff::av_error(err, Category::Encode, "cannot upload to VA-API"));
        send = hw_frame.get();
    }
    send->pts = next_pts++;
    const int err = avcodec_send_frame(codec.get(), send);
    if (err < 0)
        return std::unexpected(ff::av_error(err, Category::Encode, "cannot submit video frame"));
    return drain();
}

Result<void> VideoWriter::write(std::span<const std::uint8_t> rgb, int stride) {
    return impl_->encode(impl_->scale.get(), rgb, stride, 3);
}

Result<void> VideoWriter::write_rgba(std::span<const std::uint8_t> rgba, int stride) {
    auto& s = *impl_;
    if (!s.scale_rgba) {
        const int w = s.codec->width;
        const int h = s.codec->height;
        s.scale_rgba.reset(sws_getContext(w, h, AV_PIX_FMT_RGBA, w, h, s.sw_format, SWS_BILINEAR,
                                          nullptr, nullptr, nullptr));
        if (!s.scale_rgba || sws_setColorspaceDetails(
                                 s.scale_rgba.get(), sws_getCoefficients(SWS_CS_ITU709), 1,
                                 sws_getCoefficients(SWS_CS_ITU709), 0, 0, 1 << 16, 1 << 16) < 0) {
            return make_error(ErrorCode::Internal, Category::Encode, "cannot convert RGBA to YUV");
        }
    }
    return s.encode(s.scale_rgba.get(), rgba, stride, 4);
}

Result<void> VideoWriter::write_audio(std::span<const float> interleaved) {
    auto& s = *impl_;
    if (!s.audio || s.finished) {
        return make_error(ErrorCode::InvalidArgument, Category::Encode, "no audio track to write");
    }
    const auto channels = static_cast<std::size_t>(s.audio->ch_layout.nb_channels);
    const std::size_t frames = interleaved.size() / channels;
    if (frames == 0 || std::cmp_greater(frames, INT_MAX))
        return {};
    // FIFO input is planar like the encoder: deinterleave into one reused buffer.
    s.planar.resize(frames * channels);
    std::array<void*, 2> planes{s.planar.data(), s.planar.data() + frames};
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t ch = 0; ch < channels; ++ch)
            s.planar[(ch * frames) + i] = interleaved[(i * channels) + ch];
    }
    if (const int err = av_audio_fifo_write(s.fifo.get(), planes.data(), static_cast<int>(frames));
        std::cmp_less(err, frames)) {
        return std::unexpected(
            ff::av_error(err < 0 ? err : AVERROR(ENOMEM), Category::Encode, "cannot queue audio"));
    }
    return s.encode_audio(false);
}

Result<void> VideoWriter::finish() {
    auto& s = *impl_;
    if (s.finished)
        return make_error(ErrorCode::InvalidArgument, Category::Encode,
                          "output is already finished");
    int err = avcodec_send_frame(s.codec.get(), nullptr);
    if (err < 0)
        return std::unexpected(ff::av_error(err, Category::Encode, "cannot flush video encoder"));
    if (auto drained = s.drain(); !drained)
        return drained;
    if (s.audio) {
        if (auto encoded = s.encode_audio(true); !encoded)
            return encoded;
        if (err = avcodec_send_frame(s.audio.get(), nullptr); err < 0)
            return std::unexpected(
                ff::av_error(err, Category::Encode, "cannot flush audio encoder"));
        if (auto drained = s.drain(s.audio.get(), s.audio_stream); !drained)
            return drained;
    }
    err = av_write_trailer(s.format.get());
    if (err < 0) {
        return std::unexpected(
            ff::av_error(err, Category::Encode, "cannot finish MP4", s.file.string()));
    }
    err = avio_closep(&s.format->pb);
    if (err < 0) {
        return std::unexpected(
            ff::av_error(err, Category::Encode, "cannot close MP4", s.file.string()));
    }
    if (::rename(s.temporary.c_str(), s.file.c_str()) != 0) {
        return std::unexpected(io_error("cannot replace output", s.file, errno));
    }
    s.finished = true;
    return {};
}

} // namespace oma::media
