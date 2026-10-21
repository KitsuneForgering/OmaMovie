// S9 (M12 step 4): the real compositor's NV12 written straight into h264_vaapi's own surfaces,
// against today's route through host memory. Each frame of a video is composited once; its NV12
// goes (a) into a pool surface's imported planes with VulkanCompositor::write_nv12, and (b)
// through read_nv12 and an upload into a surface of a second pool (the current export path).
// Both encoders get identical settings, so identical inputs make identical movies; the caller
// compares them with an independent decode (framemd5).
// Usage: s9_compositor_to_encoder input.mp4 native.mp4 readback.mp4
#include "va_planes.hpp"

#include "oma/compositor/compositor.hpp"
#include "oma/media/video_decoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
}

#include <chrono>
#include <cstring>
#include <map>

namespace {

// One encoder writing one file from its own VA-API surface pool.
struct Output {
    AVBufferRef* pool = nullptr;
    AVCodecContext* enc = nullptr;
    AVFormatContext* mux = nullptr;
    AVStream* stream = nullptr;
    AVPacket* packet = av_packet_alloc();

    bool open(AVBufferRef* device, const char* path, int w, int h) {
        pool = av_hwframe_ctx_alloc(device);
        auto* fc = reinterpret_cast<AVHWFramesContext*>(pool->data);
        fc->format = AV_PIX_FMT_VAAPI;
        fc->sw_format = AV_PIX_FMT_NV12;
        fc->width = w;
        fc->height = h;
        fc->initial_pool_size = 16;
        if (av_hwframe_ctx_init(pool) < 0)
            return va_planes::fail("VA-API frame pool");
        const AVCodec* codec = avcodec_find_encoder_by_name("h264_vaapi");
        enc = avcodec_alloc_context3(codec);
        enc->width = w;
        enc->height = h;
        enc->time_base = {1, 30};
        enc->framerate = {30, 1};
        enc->pix_fmt = AV_PIX_FMT_VAAPI;
        enc->hw_frames_ctx = av_buffer_ref(pool);
        enc->max_b_frames = 0;
        enc->global_quality = 20;
        enc->color_range = AVCOL_RANGE_MPEG;
        enc->colorspace = AVCOL_SPC_BT709;
        avformat_alloc_output_context2(&mux, nullptr, nullptr, path);
        if (mux->oformat->flags & AVFMT_GLOBALHEADER)
            enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (avcodec_open2(enc, codec, nullptr) < 0)
            return va_planes::fail("h264_vaapi");
        stream = avformat_new_stream(mux, nullptr);
        avcodec_parameters_from_context(stream->codecpar, enc);
        stream->time_base = enc->time_base;
        return (avio_open(&mux->pb, path, AVIO_FLAG_WRITE) >= 0 && avformat_write_header(mux, nullptr) >= 0) ||
               va_planes::fail("output file");
    }
    bool send(AVFrame* frame) {
        if (avcodec_send_frame(enc, frame) < 0)
            return va_planes::fail("sending a frame");
        while (avcodec_receive_packet(enc, packet) == 0) {
            av_packet_rescale_ts(packet, enc->time_base, stream->time_base);
            packet->stream_index = stream->index;
            av_interleaved_write_frame(mux, packet);
        }
        return true;
    }
    void close() {
        send(nullptr);
        av_write_trailer(mux);
        avio_closep(&mux->pb);
        avformat_free_context(mux);
        avcodec_free_context(&enc);
        av_packet_free(&packet);
        av_buffer_unref(&pool);
    }
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 4)
        return 2;
    auto created = oma::gpu::Device::create();
    if (!created)
        return va_planes::fail("no Vulkan device") ? 0 : 2;
    const oma::gpu::Device& d = **created;
    oma::media::VideoDecoderOptions options;
    options.paths = {oma::media::DecodePath::Software};
    auto decoder = oma::media::VideoDecoder::open(argv[1], options);
    auto compositor = oma::compositor::VulkanCompositor::create(d);
    if (!decoder || !compositor || !(*decoder)->stream().video)
        return va_planes::fail("decoder or compositor") ? 0 : 1;
    const auto& video = *(*decoder)->stream().video;
    const int w = static_cast<int>(video.width) & ~1;
    const int h = static_cast<int>(video.height) & ~1;
    oma::compositor::RenderGraph graph;
    graph.width = static_cast<std::uint32_t>(w);
    graph.height = static_cast<std::uint32_t>(h);
    graph.layers.resize(1);

    AVBufferRef* device = nullptr;
    if (av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_VAAPI, d.info().render_node.c_str(), nullptr, 0) < 0)
        return va_planes::fail("VA-API device") ? 0 : 2;
    VADisplay va =
        reinterpret_cast<AVVAAPIDeviceContext*>(reinterpret_cast<AVHWDeviceContext*>(device->data)->hwctx)->display;
    Output native;
    Output readback;
    if (!native.open(device, argv[2], w, h) || !readback.open(device, argv[3], w, h))
        return 1;
    std::map<VASurfaceID, std::array<va_planes::Plane, 2>> imported;
    int frames = 0;
    double native_ms = 0;   // getting a surface, (first use: importing it), write_nv12
    double readback_ms = 0; // read_nv12, then uploading into a surface
    const auto ms_since = [](std::chrono::steady_clock::time_point t) {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
    };
    for (;;) {
        auto next = (*decoder)->next();
        if (!next)
            return va_planes::fail("decode") ? 0 : 1;
        if (!*next)
            break;
        const std::array<oma::compositor::LayerInput, 1> inputs{
            oma::compositor::LayerInput{.frame = &**next, .color = video.color}};
        if (!(*compositor)->render(graph, inputs))
            return va_planes::fail("render") ? 0 : 1;

        // (a) GPU-native: the compositor writes the encoder's surface.
        const auto native_start = std::chrono::steady_clock::now();
        AVFrame* a = av_frame_alloc();
        if (av_hwframe_get_buffer(native.pool, a, 0) < 0)
            return va_planes::fail("native pool surface") ? 0 : 1;
        const auto surface = static_cast<VASurfaceID>(reinterpret_cast<std::uintptr_t>(a->data[3]));
        auto it = imported.find(surface);
        if (it == imported.end()) {
            VADRMPRIMESurfaceDescriptor desc{};
            if (vaExportSurfaceHandle(va, surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                      &desc) != VA_STATUS_SUCCESS)
                return va_planes::fail("exporting a pool surface") ? 0 : 1;
            std::array<va_planes::Plane, 2> planes;
            const auto uw = static_cast<std::uint32_t>(w);
            const auto uh = static_cast<std::uint32_t>(h);
            if (!va_planes::import_plane(d, desc, 0, VK_FORMAT_R8_UNORM, uw, uh, planes[0]) ||
                !va_planes::import_plane(d, desc, 1, VK_FORMAT_R8G8_UNORM, uw / 2, uh / 2, planes[1]))
                return 1;
            for (std::uint32_t o = 0; o < desc.num_objects; ++o)
                close(desc.objects[o].fd);
            it = imported.emplace(surface, planes).first;
        }
        vaSyncSurface(va, surface);
        const auto& p = it->second;
        if (!(*compositor)->write_nv12({.luma = p[0].image, .luma_view = p[0].view, .cbcr = p[1].image,
                                        .cbcr_view = p[1].view, .foreign = true}))
            return va_planes::fail("write_nv12 into the surface") ? 0 : 1;
        native_ms += ms_since(native_start);
        a->pts = frames;
        a->color_range = AVCOL_RANGE_MPEG;
        a->colorspace = AVCOL_SPC_BT709;

        // (b) Today's route: read the NV12 back and upload it into the other encoder's surface.
        const auto readback_start = std::chrono::steady_clock::now();
        auto nv12 = (*compositor)->read_nv12();
        if (!nv12)
            return va_planes::fail("read_nv12") ? 0 : 1;
        AVFrame* sw = av_frame_alloc();
        sw->format = AV_PIX_FMT_NV12;
        sw->width = w;
        sw->height = h;
        sw->data[0] = nv12->bytes.data();
        sw->data[1] = nv12->bytes.data() + (static_cast<std::size_t>(nv12->stride) * nv12->height);
        sw->linesize[0] = sw->linesize[1] = static_cast<int>(nv12->stride);
        AVFrame* b = av_frame_alloc();
        if (av_hwframe_get_buffer(readback.pool, b, 0) < 0 || av_hwframe_transfer_data(b, sw, 0) < 0)
            return va_planes::fail("uploading the readback") ? 0 : 1;
        av_frame_free(&sw);
        readback_ms += ms_since(readback_start);
        b->pts = frames;
        b->color_range = AVCOL_RANGE_MPEG;
        b->colorspace = AVCOL_SPC_BT709;

        if (!native.send(a) || !readback.send(b))
            return 1;
        av_frame_free(&a);
        av_frame_free(&b);
        ++frames;
    }
    native.close();
    readback.close();
    for (auto& [surface, planes] : imported)
        for (va_planes::Plane& plane : planes)
            va_planes::destroy(d, plane);
    av_buffer_unref(&device);
    std::printf("encoded %d frames %dx%d both ways (%zu native surfaces); per frame: native %.2f ms, "
                "readback + upload %.2f ms\n",
                frames, w, h, imported.size(), native_ms / frames, readback_ms / frames);
    return 0;
}
