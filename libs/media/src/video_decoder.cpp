#include "oma/media/video_decoder.hpp"

#include "ffmpeg.hpp"
#include "stream_info.hpp"
#include "video_frame_impl.hpp"
#include "vulkan_device.hpp"

#include "oma/base/log.hpp"
#include "oma/gpu/device.hpp"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <format>
#include <string>
#include <utility>

namespace oma::media {

namespace {

// Extra surfaces so a frame held by a consumer (and seek's one-frame lookahead) never starves
// the decoder.
constexpr int kExtraHwFrames = 4;

VkVideoCodecOperationFlagsKHR vulkan_decode_op(AVCodecID id) noexcept {
    switch (id) {
    case AV_CODEC_ID_H264:
        return VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    case AV_CODEC_ID_HEVC:
        return VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
    case AV_CODEC_ID_AV1:
        return VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
    case AV_CODEC_ID_VP9:
        return VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
    default:
        return 0;
    }
}

bool device_decodes(const gpu::Device& device, AVCodecID id) {
    const VkVideoCodecOperationFlagsKHR op = vulkan_decode_op(id);
    return op != 0 && std::ranges::any_of(device.queue_families(), [&](const gpu::QueueFamily& f) {
               return (f.video_codecs & op) != 0;
           });
}

bool codec_supports(const AVCodec& codec, AVHWDeviceType type) {
    for (int i = 0;; ++i) {
        const AVCodecHWConfig* cfg = avcodec_get_hw_config(&codec, i);
        if (cfg == nullptr) {
            return false;
        }
        if (cfg->device_type == type &&
            (static_cast<unsigned>(cfg->methods) &
             static_cast<unsigned>(AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) != 0) {
            return true;
        }
    }
}

Error unsupported(std::string message, const std::string& context = {}) {
    return {ErrorCode::Unsupported, Category::Decode, std::move(message), context};
}

struct SwsFreer {
    void operator()(SwsContext* p) const noexcept { sws_freeContext(p); }
};

// Consumers read YUV or planar GBR (SampleLayout); packed RGB, palette and grey frames (PNG,
// JPEG, RGB codecs) are rearranged into planar GBR. Alpha is dropped for now.
AVPixelFormat planar_rgb_for(AVPixelFormat format) {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format);
    if (desc == nullptr || (desc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0) {
        return AV_PIX_FMT_NONE;
    }
    const bool rgb_like =
        (desc->flags & (AV_PIX_FMT_FLAG_RGB | AV_PIX_FMT_FLAG_PAL)) != 0 || desc->nb_components < 3;
    if (!rgb_like || format == AV_PIX_FMT_GBRP || format == AV_PIX_FMT_GBRP16) {
        return AV_PIX_FMT_NONE;
    }
    return desc->comp[0].depth > 8 ? AV_PIX_FMT_GBRP16 : AV_PIX_FMT_GBRP;
}

} // namespace

struct VideoDecoder::Impl {
    std::filesystem::path file;
    const gpu::Device* device = nullptr;
    int threads = 0;
    bool allow_software = true;

    ff::FormatPtr fmt;
    AVStream* st = nullptr;
    StreamInfo info;
    ff::CodecPtr dec;
    ff::PacketPtr pkt;
    DecodePath path = DecodePath::Software;

    // Hardware state.
    AVPixelFormat wanted_hw = AV_PIX_FMT_NONE;
    bool hw_rejected = false; // the driver refused the stream; set from get_format
    ff::BufferPtr vk_device;  // FFmpeg's view of OmaMovie's device
    ff::BufferPtr vk_frames;  // Vulkan frames derived from the VA-API pool
    const std::uint8_t* vk_frames_source = nullptr; // data of the VA-API frames context

    // Decode position.
    bool draining = false;
    std::int64_t frames_delivered = 0;
    std::optional<RationalTime> seek_target;
    std::optional<RationalTime> last_seek; // where a software fallback restarts
    ff::FramePtr pending;                  // first frame past a seek target, returned next
    std::unique_ptr<SwsContext, SwsFreer> to_planar_rgb; // reused while the format holds

    [[nodiscard]] Result<void> open_path(DecodePath candidate);
    [[nodiscard]] Result<void> reposition();
    [[nodiscard]] Result<void> fall_back_to_software();
    [[nodiscard]] Result<ff::FramePtr> receive();
    [[nodiscard]] Result<VideoFrame> finish(ff::FramePtr frame);
    [[nodiscard]] Result<ff::FramePtr> map_to_vulkan(const AVFrame& src);

    static AVPixelFormat pick_format(AVCodecContext* ctx, const AVPixelFormat* formats);
};

AVPixelFormat VideoDecoder::Impl::pick_format(AVCodecContext* ctx, const AVPixelFormat* formats) {
    auto* d = static_cast<Impl*>(ctx->opaque);
    for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == d->wanted_hw) {
            return *p;
        }
    }
    // FFmpeg would quietly continue in software; the decoder reopens instead (see next()).
    d->hw_rejected = true;
    return AV_PIX_FMT_NONE;
}

Result<void> VideoDecoder::Impl::open_path(DecodePath candidate) {
    const AVCodecID id = st->codecpar->codec_id;
    const AVCodec* codec = avcodec_find_decoder(id);
    if (candidate != DecodePath::Software) {
        if (device == nullptr) {
            return std::unexpected(unsupported("no GPU device"));
        }
        // libdav1d is FFmpeg's preferred AV1 decoder but has no hwaccel (S2).
        if (id == AV_CODEC_ID_AV1) {
            codec = avcodec_find_decoder_by_name("av1");
        }
    }
    if (codec == nullptr) {
        return std::unexpected(unsupported("no decoder", avcodec_get_name(id)));
    }

    ff::BufferPtr hw_device;
    wanted_hw = AV_PIX_FMT_NONE;
    if (candidate == DecodePath::VaapiToVulkan) {
        if (!device->has_extension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) ||
            !device->has_extension(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)) {
            return std::unexpected(unsupported("device cannot import DMA-BUF"));
        }
        if (!codec_supports(*codec, AV_HWDEVICE_TYPE_VAAPI)) {
            return std::unexpected(unsupported("no VA-API hwaccel for codec", codec->name));
        }
        // Decode on the GPU that composites (hybrid laptops): its render node, when the driver
        // reports one; otherwise libva's default device, which matches on single-GPU machines.
        const std::string& node = device->info().render_node;
        if (node.empty()) {
            log_warn(Category::Decode,
                     "{}: Vulkan device has no render node, VA-API uses the "
                     "default device",
                     file.filename().string());
        }
        AVBufferRef* va = nullptr;
        if (const int err = av_hwdevice_ctx_create(
                &va, AV_HWDEVICE_TYPE_VAAPI, node.empty() ? nullptr : node.c_str(), nullptr, 0);
            err < 0) {
            return std::unexpected(ff::av_error(err, Category::Decode, "no VA-API device"));
        }
        hw_device.reset(va);
        wanted_hw = AV_PIX_FMT_VAAPI;
    } else if (candidate == DecodePath::VulkanVideo) {
        if (!device_decodes(*device, id) || !codec_supports(*codec, AV_HWDEVICE_TYPE_VULKAN)) {
            return std::unexpected(unsupported("no Vulkan Video decode for codec", codec->name));
        }
        wanted_hw = AV_PIX_FMT_VULKAN;
    }
    if (candidate != DecodePath::Software && !vk_device) {
        auto wrapped = ff::wrap_vulkan_device(*device);
        if (!wrapped) {
            return std::unexpected(wrapped.error());
        }
        vk_device = std::move(*wrapped);
    }
    if (candidate == DecodePath::VulkanVideo) {
        hw_device.reset(av_buffer_ref(vk_device.get()));
    }

    auto ctx = ff::alloc_codec(*st, *codec);
    if (!ctx) {
        return std::unexpected(ctx.error());
    }
    (*ctx)->opaque = this;
    if (hw_device) {
        (*ctx)->hw_device_ctx = av_buffer_ref(hw_device.get());
        (*ctx)->get_format = pick_format;
        (*ctx)->extra_hw_frames = kExtraHwFrames;
    } else {
        (*ctx)->thread_count = threads;
    }
    if (const int err = avcodec_open2(ctx->get(), codec, nullptr); err < 0) {
        return std::unexpected(
            ff::av_error(err, Category::Decode, "cannot open decoder", codec->name));
    }
    dec = std::move(*ctx);
    path = candidate;
    hw_rejected = false;
    vk_frames.reset();
    vk_frames_source = nullptr;
    return {};
}

// Moves the demuxer to the keyframe before the seek target (or the start) and resets decoding.
Result<void> VideoDecoder::Impl::reposition() {
    std::int64_t ts = st->start_time != AV_NOPTS_VALUE ? st->start_time : 0;
    if (seek_target) {
        auto ticks = ff::to_ticks(*seek_target, info.timebase, Rounding::Floor);
        if (!ticks) {
            return std::unexpected(ticks.error());
        }
        ts = *ticks;
    }
    if (const int err = av_seek_frame(fmt.get(), st->index, ts, AVSEEK_FLAG_BACKWARD); err < 0) {
        // Some streams cannot seek backward past their first keyframe: retry from the start.
        if (const int err2 = av_seek_frame(fmt.get(), st->index, INT64_MIN, AVSEEK_FLAG_BACKWARD);
            err2 < 0) {
            return std::unexpected(
                ff::av_error(err, Category::Decode, "seek failed", file.string()));
        }
    }
    avcodec_flush_buffers(dec.get());
    draining = false;
    pending.reset();
    return {};
}

Result<void> VideoDecoder::Impl::fall_back_to_software() {
    const DecodePath refused = path;
    if (frames_delivered > 0) {
        return std::unexpected(unsupported(
            std::format("{} decode stopped mid-stream", to_string(refused)), file.string()));
    }
    if (!allow_software) {
        return std::unexpected(
            unsupported(std::format("{} decode refused the stream and software is not allowed",
                                    to_string(refused)),
                        file.string()));
    }
    log_warn(Category::Decode, "{}: {} decode refused {} ({}), falling back to software",
             file.filename().string(), to_string(refused), info.codec, info.profile);
    if (auto r = open_path(DecodePath::Software); !r) {
        return r;
    }
    seek_target = last_seek;
    return reposition();
}

// The next decoded frame in presentation order, or nullptr at the end of the stream.
Result<ff::FramePtr> VideoDecoder::Impl::receive() {
    ff::FramePtr frame(av_frame_alloc());
    if (!frame) {
        return make_error(ErrorCode::Internal, Category::Decode, "cannot allocate a frame");
    }
    for (;;) {
        const int r = avcodec_receive_frame(dec.get(), frame.get());
        if (r == 0) {
            return frame;
        }
        if (r == AVERROR_EOF) {
            return ff::FramePtr();
        }
        if (hw_rejected) {
            return ff::FramePtr();
        }
        if (r != AVERROR(EAGAIN)) {
            return std::unexpected(
                ff::av_error(r, Category::Decode, "decode failed", file.string()));
        }
        if (draining) {
            return ff::FramePtr();
        }
        const int rd = av_read_frame(fmt.get(), pkt.get());
        if (rd < 0) {
            if (rd != AVERROR_EOF) {
                // A truncated or damaged file: decode what was read, then end the stream.
                log_warn(Category::Decode, "{}: read error, ending the stream: {}",
                         file.filename().string(), ff::error_string(rd));
            }
            draining = true;
            avcodec_send_packet(dec.get(), nullptr);
            continue;
        }
        if (pkt->stream_index != st->index) {
            av_packet_unref(pkt.get());
            continue;
        }
        const int sr = avcodec_send_packet(dec.get(), pkt.get());
        av_packet_unref(pkt.get());
        if (hw_rejected) {
            return ff::FramePtr();
        }
        if (sr < 0 && sr != AVERROR(EAGAIN)) {
            if (sr == AVERROR_INVALIDDATA) {
                log_warn(Category::Decode, "{}: skipping a damaged packet",
                         file.filename().string());
                continue;
            }
            return std::unexpected(
                ff::av_error(sr, Category::Decode, "decode failed", file.string()));
        }
    }
}

Result<ff::FramePtr> VideoDecoder::Impl::map_to_vulkan(const AVFrame& src) {
    // Every frame carries its own reference; the pool is the referenced data.
    if (!vk_frames || vk_frames_source != src.hw_frames_ctx->data) {
        // The decoder recreates its pool on size changes; derive a matching Vulkan pool.
        AVBufferRef* derived = nullptr;
        const int err = av_hwframe_ctx_create_derived(&derived, AV_PIX_FMT_VULKAN, vk_device.get(),
                                                      src.hw_frames_ctx, 0);
        if (err < 0) {
            return std::unexpected(
                ff::av_error(err, Category::Decode, "cannot derive Vulkan frames from VA-API"));
        }
        vk_frames.reset(derived);
        vk_frames_source = src.hw_frames_ctx->data;
    }
    ff::FramePtr out(av_frame_alloc());
    if (!out) {
        return make_error(ErrorCode::Internal, Category::Decode, "cannot allocate a frame");
    }
    out->format = AV_PIX_FMT_VULKAN;
    out->hw_frames_ctx = av_buffer_ref(vk_frames.get());
    // AV_HWFRAME_MAP_DIRECT fails with EINVAL in FFmpeg 9; the plain mapping still imports the
    // DMA-BUF without a copy (S2).
    if (const int err = av_hwframe_map(out.get(), &src, AV_HWFRAME_MAP_READ); err < 0) {
        return std::unexpected(ff::av_error(err, Category::Decode, "cannot map VA-API to Vulkan"));
    }
    return out;
}

Result<VideoFrame> VideoDecoder::Impl::finish(ff::FramePtr frame) {
    auto impl = std::make_unique<VideoFrame::Impl>();
    impl->path = path;
    impl->pts = ff::to_time(frame->best_effort_timestamp, info.timebase);
    if (frame->duration > 0) {
        impl->duration = ff::to_time(frame->duration, info.timebase);
    }
    if (frame->format == AV_PIX_FMT_VAAPI) {
        auto mapped = map_to_vulkan(*frame);
        if (!mapped) {
            return std::unexpected(mapped.error());
        }
        impl->frame = std::move(*mapped);
    } else if (const AVPixelFormat planar =
                   planar_rgb_for(static_cast<AVPixelFormat>(frame->format));
               planar != AV_PIX_FMT_NONE) {
        // ponytail: one new frame per conversion; pool it if RGB video ever plays in real time.
        to_planar_rgb.reset(
            sws_getCachedContext(to_planar_rgb.release(), frame->width, frame->height,
                                 static_cast<AVPixelFormat>(frame->format), frame->width,
                                 frame->height, planar, SWS_POINT, nullptr, nullptr, nullptr));
        ff::FramePtr out(av_frame_alloc());
        if (!to_planar_rgb || !out) {
            return std::unexpected(unsupported("cannot convert this RGB format", file.string()));
        }
        out->format = planar;
        out->width = frame->width;
        out->height = frame->height;
        if (const int err = sws_scale_frame(to_planar_rgb.get(), out.get(), frame.get()); err < 0) {
            return std::unexpected(ff::av_error(err, Category::Decode, "RGB conversion failed"));
        }
        av_frame_copy_props(out.get(), frame.get());
        impl->frame = std::move(out);
    } else {
        impl->frame = std::move(frame);
    }
    if (impl->frame->format == AV_PIX_FMT_VULKAN) {
        impl->layout =
            reinterpret_cast<const AVHWFramesContext*>(impl->frame->hw_frames_ctx->data)->sw_format;
    } else {
        impl->layout = static_cast<AVPixelFormat>(impl->frame->format);
    }
    ++frames_delivered;
    return VideoFrame(std::move(impl));
}

// --------------------------------------------------------------------------- VideoDecoder

VideoDecoder::VideoDecoder(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
VideoDecoder::~VideoDecoder() = default;

Result<std::unique_ptr<VideoDecoder>> VideoDecoder::open(const std::filesystem::path& path,
                                                         const VideoDecoderOptions& options) {
    auto impl = std::make_unique<Impl>();
    impl->file = path;
    impl->device = options.device;
    impl->threads = options.threads;
    impl->allow_software = std::ranges::contains(options.paths, DecodePath::Software);

    auto fmt = ff::open_input(path);
    if (!fmt) {
        return std::unexpected(fmt.error());
    }
    impl->fmt = std::move(*fmt);
    const int index = options.stream.value_or(
        av_find_best_stream(impl->fmt.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0));
    if (index < 0 || std::cmp_greater_equal(index, impl->fmt->nb_streams) ||
        impl->fmt->streams[index]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
        return make_error(ErrorCode::InvalidArgument, Category::Decode, "no such video stream",
                          path.string());
    }
    impl->st = impl->fmt->streams[index];
    // Only decode this stream; the demuxer drops other packets early.
    for (unsigned i = 0; i < impl->fmt->nb_streams; ++i) {
        impl->fmt->streams[i]->discard =
            std::cmp_equal(i, index) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }
    const auto tb = ff::to_timebase(impl->st->time_base);
    if (!tb) {
        return make_error(ErrorCode::InvalidData, Category::Decode, "stream has no timebase",
                          path.string());
    }
    impl->info = describe_stream(*impl->st, impl->fmt->iformat->name);
    impl->pkt.reset(av_packet_alloc());
    if (!impl->pkt) {
        return make_error(ErrorCode::Internal, Category::Decode, "cannot allocate a packet");
    }

    // ADR-0006: no tone mapping yet; HDR is shown through the SDR path and said so once.
    if (impl->info.video && (impl->info.video->color.transfer == AVCOL_TRC_SMPTE2084 ||
                             impl->info.video->color.transfer == AVCOL_TRC_ARIB_STD_B67)) {
        log_warn(Category::Decode, "{}: HDR ({}) is shown as SDR without tone mapping",
                 path.filename().string(),
                 impl->info.video->color.transfer == AVCOL_TRC_SMPTE2084 ? "PQ" : "HLG");
    }

    std::string refusals;
    for (const DecodePath candidate : options.paths) {
        auto opened = impl->open_path(candidate);
        if (opened) {
            log_info(Category::Decode, "{}: {} {} decode", path.filename().string(),
                     impl->info.codec, to_string(candidate));
            return std::unique_ptr<VideoDecoder>(new VideoDecoder(std::move(impl)));
        }
        log_debug(Category::Decode, "{}: {} decode unavailable: {}", path.filename().string(),
                  to_string(candidate), opened.error().summary());
        refusals += std::format("{}{}: {}", refusals.empty() ? "" : "; ", to_string(candidate),
                                opened.error().message());
    }
    return std::unexpected(unsupported("no decode path available", refusals));
}

const StreamInfo& VideoDecoder::stream() const noexcept {
    return impl_->info;
}

DecodePath VideoDecoder::path() const noexcept {
    return impl_->path;
}

Result<std::optional<VideoFrame>> VideoDecoder::next() {
    Impl& d = *impl_;
    if (d.pending) {
        auto f = d.finish(std::move(d.pending));
        if (!f) {
            return std::unexpected(f.error());
        }
        return std::optional<VideoFrame>(std::move(*f));
    }

    ff::FramePtr candidate; // in a seek: the last frame at or before the target so far
    for (;;) {
        auto received = d.receive();
        if (d.hw_rejected) {
            if (auto r = d.fall_back_to_software(); !r) {
                return std::unexpected(r.error());
            }
            candidate.reset();
            continue;
        }
        if (!received) {
            return std::unexpected(received.error());
        }
        ff::FramePtr frame = std::move(*received);
        if (!frame) { // end of stream
            d.seek_target.reset();
            if (!candidate) {
                return std::optional<VideoFrame>();
            }
            frame = std::move(candidate);
        } else if (d.seek_target) {
            const auto pts = ff::to_time(frame->best_effort_timestamp, d.info.timebase);
            if (pts && *pts <= *d.seek_target) {
                candidate = std::move(frame);
                continue;
            }
            d.seek_target.reset();
            if (candidate) {
                d.pending = std::move(frame);
                frame = std::move(candidate);
            }
        }

        auto finished = d.finish(std::move(frame));
        if (!finished && d.path == DecodePath::VaapiToVulkan && d.frames_delivered == 0) {
            // The import into Vulkan failed (format or modifier the device cannot take).
            log_warn(Category::Decode, "{}", finished.error().summary());
            d.hw_rejected = true;
            if (auto r = d.fall_back_to_software(); !r) {
                return std::unexpected(r.error());
            }
            continue;
        }
        if (!finished) {
            return std::unexpected(finished.error());
        }
        return std::optional<VideoFrame>(std::move(*finished));
    }
}

Result<void> VideoDecoder::seek(const RationalTime& t) {
    impl_->seek_target = t;
    impl_->last_seek = t;
    return impl_->reposition();
}

} // namespace oma::media
