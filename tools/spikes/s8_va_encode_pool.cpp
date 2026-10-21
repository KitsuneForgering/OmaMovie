// S8 (M12 step 3): encode surfaces the GPU wrote. FFmpeg's h264_vaapi allocates its own NV12
// surface pool; each frame's surface is exported as DMA-BUF, its planes imported once into
// OmaMovie's Vulkan device, cleared on the GPU to a gray that changes per frame, and the same
// AVFrame is sent to the encoder: no host copy and no upload. The output is checked by the caller
// with an independent decode (Docs/Research/evidence/2026-10-21-m12-encoder-input-routes.md).
// Usage: s8_va_encode_pool out.mp4 [frames=120]
#include "va_planes.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
}

#include <cstdlib>
#include <map>

namespace {

constexpr int kW = 1920;
constexpr int kH = 1080;

// The gray of frame `i` (limited-range luma 16-235), neutral chroma.
std::uint8_t luma(int i) {
    return static_cast<std::uint8_t>(16 + ((i * 7) % 220));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2)
        return 2;
    const int frames = argc > 2 ? std::atoi(argv[2]) : 120;
    auto created = oma::gpu::Device::create();
    if (!created)
        return va_planes::fail("no Vulkan device") ? 0 : 2;
    const oma::gpu::Device& d = **created;

    AVBufferRef* device = nullptr;
    if (av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VAAPI, d.info().render_node.c_str(), nullptr, 0) < 0)
        return va_planes::fail("VA-API device") ? 0 : 2;
    VADisplay va = reinterpret_cast<AVVAAPIDeviceContext*>(reinterpret_cast<AVHWDeviceContext*>(device->data)->hwctx)
                       ->display;
    AVBufferRef* pool = av_hwframe_ctx_alloc(device);
    auto* fc = reinterpret_cast<AVHWFramesContext*>(pool->data);
    fc->format = AV_PIX_FMT_VAAPI;
    fc->sw_format = AV_PIX_FMT_NV12;
    fc->width = kW;
    fc->height = kH;
    fc->initial_pool_size = 16;
    if (av_hwframe_ctx_init(pool) < 0)
        return va_planes::fail("VA-API frame pool") ? 0 : 1;

    const AVCodec* codec = avcodec_find_encoder_by_name("h264_vaapi");
    AVCodecContext* enc = avcodec_alloc_context3(codec);
    enc->width = kW;
    enc->height = kH;
    enc->time_base = {1, 60};
    enc->framerate = {60, 1};
    enc->pix_fmt = AV_PIX_FMT_VAAPI;
    enc->hw_frames_ctx = av_buffer_ref(pool);
    enc->max_b_frames = 0;
    enc->global_quality = 20; // constant QP
    enc->color_range = AVCOL_RANGE_MPEG;
    AVFormatContext* mux = nullptr;
    avformat_alloc_output_context2(&mux, nullptr, nullptr, argv[1]);
    if (mux->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(enc, codec, nullptr) < 0)
        return va_planes::fail("h264_vaapi on the pool") ? 0 : 1;
    AVStream* stream = avformat_new_stream(mux, nullptr);
    avcodec_parameters_from_context(stream->codecpar, enc);
    stream->time_base = enc->time_base;
    if (avio_open(&mux->pb, argv[1], AVIO_FLAG_WRITE) < 0 || avformat_write_header(mux, nullptr) < 0)
        return va_planes::fail("output file") ? 0 : 1;

    std::map<VASurfaceID, std::array<va_planes::Plane, 2>> imported; // one import per pool surface
    AVPacket* packet = av_packet_alloc();
    const auto drain = [&] {
        while (avcodec_receive_packet(enc, packet) == 0) {
            av_packet_rescale_ts(packet, enc->time_base, stream->time_base);
            packet->stream_index = stream->index;
            av_interleaved_write_frame(mux, packet);
        }
    };
    for (int i = 0; i <= frames; ++i) {
        AVFrame* frame = nullptr;
        if (i < frames) {
            frame = av_frame_alloc();
            if (av_hwframe_get_buffer(pool, frame, 0) < 0)
                return va_planes::fail("a pool surface") ? 0 : 1;
            const auto surface = static_cast<VASurfaceID>(reinterpret_cast<std::uintptr_t>(frame->data[3]));
            auto it = imported.find(surface);
            if (it == imported.end()) {
                VADRMPRIMESurfaceDescriptor desc{};
                if (vaExportSurfaceHandle(va, surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                          VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                          &desc) != VA_STATUS_SUCCESS)
                    return va_planes::fail("exporting a pool surface") ? 0 : 1;
                std::array<va_planes::Plane, 2> planes;
                if (!va_planes::import_plane(d, desc, 0, VK_FORMAT_R8_UNORM, kW, kH, planes[0]) ||
                    !va_planes::import_plane(d, desc, 1, VK_FORMAT_R8G8_UNORM, kW / 2, kH / 2, planes[1]))
                    return 1;
                for (std::uint32_t o = 0; o < desc.num_objects; ++o)
                    close(desc.objects[o].fd); // Vulkan holds its own references now
                it = imported.emplace(surface, planes).first;
            }
            // The encoder may still read this surface from an earlier frame: wait for it.
            vaSyncSurface(va, surface);
            if (!va_planes::clear_planes(d, it->second, luma(i), 128, 128))
                return 1;
            frame->pts = i;
            frame->color_range = AVCOL_RANGE_MPEG;
        }
        if (avcodec_send_frame(enc, frame) < 0)
            return va_planes::fail("sending a frame") ? 0 : 1;
        av_frame_free(&frame);
        drain();
    }
    av_write_trailer(mux);
    std::printf("encoded %d GPU-written frames from %zu pool surfaces to %s\n", frames, imported.size(), argv[1]);
    for (auto& [surface, planes] : imported)
        for (va_planes::Plane& p : planes) {
            vkDestroyImage(d.device(), p.image, nullptr);
            vkFreeMemory(d.device(), p.memory, nullptr);
        }
    av_packet_free(&packet);
    avio_closep(&mux->pb);
    avformat_free_context(mux);
    avcodec_free_context(&enc);
    av_buffer_unref(&pool);
    av_buffer_unref(&device);
    return 0;
}
