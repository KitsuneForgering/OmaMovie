// S6: libplacebo on OmaMovie's device and images. Usage: s6_libplacebo <1080p-video>
// 1. Imports gpu::Device into libplacebo (no second device).
// 2. Wraps the compositor's RGBA16F output and encodes it for an SDR display; compares the
//    pixels and the cost with VulkanCompositor::encode_display.
// 3. Wraps a VA-API frame's planes (FFmpeg's timeline semaphores) and renders YUV->RGB with a
//    downscale to 1280x720 at libplacebo's fast/default/high-quality presets; compares the cost
//    with the compositor doing the same.
// Disposable; see Docs/spikes/S6-libplacebo.md.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"
#include "oma/media/video_decoder.hpp"

#include <libplacebo/log.h>
#include <libplacebo/renderer.h>
#include <libplacebo/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

using namespace oma;

namespace {

using Clock = std::chrono::steady_clock;

double median_ms(int runs, const std::function<void()>& work) {
    std::vector<double> ms;
    for (int i = 0; i < runs + 3; ++i) { // 3 warm-up runs
        const auto t0 = Clock::now();
        work();
        const double t = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        if (i >= 3) {
            ms.push_back(t);
        }
    }
    std::ranges::sort(ms);
    return ms[ms.size() / 2];
}

void lock(void* ctx, uint32_t qf, uint32_t qidx) {
    static_cast<const gpu::Device*>(ctx)->lock_queue(qf, qidx);
}
void unlock(void* ctx, uint32_t qf, uint32_t qidx) {
    static_cast<const gpu::Device*>(ctx)->unlock_queue(qf, qidx);
}

// libplacebo's parameter macros are C99 compound literals; C++ fills the structs instead.
pl_tex wrap(pl_gpu gpu, VkImage image, int w, int h, VkFormat format, VkImageUsageFlags usage) {
    pl_vulkan_wrap_params p{};
    p.image = image;
    p.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    p.width = w;
    p.height = h;
    p.format = format;
    p.usage = usage;
    return pl_vulkan_wrap(gpu, &p);
}

void release(pl_gpu gpu, pl_tex tex, VkImageLayout layout, pl_vulkan_sem sem) {
    pl_vulkan_release_params p{};
    p.tex = tex;
    p.layout = layout;
    p.qf = VK_QUEUE_FAMILY_IGNORED;
    p.semaphore = sem;
    pl_vulkan_release_ex(gpu, &p);
}

pl_tex make_tex(pl_gpu gpu, int w, int h, const char* format, bool readable) {
    pl_tex_params p{};
    p.w = w;
    p.h = h;
    p.format = pl_find_named_fmt(gpu, format);
    p.renderable = true;
    p.storable = !readable;
    p.host_readable = readable;
    return pl_tex_create(gpu, &p);
}

pl_frame rgb_frame(pl_tex tex, pl_color_transfer transfer) {
    pl_frame f{};
    f.num_planes = 1;
    f.planes[0].texture = tex;
    f.planes[0].components = 4;
    f.planes[0].component_mapping[0] = 0;
    f.planes[0].component_mapping[1] = 1;
    f.planes[0].component_mapping[2] = 2;
    f.planes[0].component_mapping[3] = 3;
    f.repr = pl_color_repr_rgb;
    f.color.primaries = PL_COLOR_PRIM_BT_709;
    f.color.transfer = transfer;
    return f;
}

pl_tex wrap(pl_gpu gpu, const gpu::Image& image) {
    const auto& d = image.desc();
    return wrap(gpu, image.handle(), static_cast<int>(d.width), static_cast<int>(d.height),
                d.format, d.usage);
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <1080p-video>\n", argv[0]);
        return 2;
    }
    // libplacebo retrieves queues with vkGetDeviceQueue, like Qt (S4): zero creation flags,
    // with every submitter bracketing the queue through Device::lock_queue (the hooks below).
    gpu::DeviceOptions options;
    options.internally_synchronized_queues = false;
    auto device = gpu::Device::create(options);
    if (!device) {
        std::fprintf(stderr, "no device: %s\n", device.error().summary().c_str());
        return 1;
    }
    const gpu::Device& dev = **device;

    // 1. Import.
    pl_log_params log_params{};
    log_params.log_cb = pl_log_color;
    log_params.log_level = PL_LOG_WARN;
    pl_log log = pl_log_create(PL_API_VER, &log_params);
    const auto exts = dev.enabled_extensions();
    const uint32_t family = dev.graphics_family();
    uint32_t queues = 1;
    for (const auto& f : dev.queue_families()) {
        if (f.index == family) {
            queues = f.count;
        }
    }
    pl_vulkan_import_params ip{};
    ip.instance = dev.instance();
    ip.get_proc_addr = dev.instance_proc_addr();
    ip.phys_device = dev.physical_device();
    ip.device = dev.device();
    ip.extensions = exts.data();
    ip.num_extensions = static_cast<int>(exts.size());
    ip.queue_graphics = {.index = family, .count = queues};
    ip.queue_compute = ip.queue_graphics;
    ip.queue_transfer = ip.queue_graphics;
    ip.features = &dev.enabled_features();
    ip.lock_queue = lock;
    ip.unlock_queue = unlock;
    ip.queue_ctx = const_cast<gpu::Device*>(&dev);
    pl_vulkan vk = pl_vulkan_import(log, &ip);
    std::printf("== 1. pl_vulkan_import on OmaMovie's device (%zu device extensions): %s\n",
                exts.size(), vk != nullptr ? "OK" : "FAILED");
    if (vk == nullptr) {
        return 1;
    }
    pl_gpu gpu = vk->gpu;
    pl_renderer renderer = pl_renderer_create(log, gpu);

    // Decode one frame twice: software (for the compositor output) and VA-API (for 3).
    media::VideoDecoderOptions sw;
    sw.paths = {media::DecodePath::Software};
    auto sw_dec = media::VideoDecoder::open(argv[1], sw);
    media::VideoDecoderOptions hw;
    hw.device = &dev;
    hw.paths = {media::DecodePath::VaapiToVulkan};
    auto hw_dec = media::VideoDecoder::open(argv[1], hw);
    if (!sw_dec || !hw_dec) {
        std::fprintf(stderr, "cannot open %s in software and VA-API\n", argv[1]);
        return 1;
    }
    auto sw_frame = (*sw_dec)->next();
    auto hw_frame = (*hw_dec)->next();
    if (!sw_frame || !*sw_frame || !hw_frame || !*hw_frame || !(*hw_frame)->on_gpu()) {
        std::fprintf(stderr, "no frames\n");
        return 1;
    }
    const media::ColorInfo color = (*sw_dec)->stream().video->color;

    // 2. Display transform of the compositor output.
    auto comp = compositor::VulkanCompositor::create(dev);
    compositor::RenderGraph g;
    g.width = 1920;
    g.height = 1080;
    g.layers.push_back(compositor::Layer{});
    const std::array<compositor::LayerInput, 1> sw_input{
        compositor::LayerInput{.frame = &**sw_frame, .color = color}};
    if (!comp || !(*comp)->render(g, sw_input)) {
        std::fprintf(stderr, "compositor failed\n");
        return 1;
    }
    const double ours_encode = median_ms(20, [&] { static_cast<void>((*comp)->encode_display()); });
    auto ours = (*comp)->read_display();

    pl_tex src = wrap(gpu, *(*comp)->output());
    release(gpu, src, VK_IMAGE_LAYOUT_GENERAL, {});
    pl_tex dst = make_tex(gpu, 1920, 1080, "rgba8", true);
    pl_frame in = rgb_frame(src, PL_COLOR_TRC_LINEAR);
    in.repr.alpha = PL_ALPHA_PREMULTIPLIED;
    pl_frame out = rgb_frame(dst, PL_COLOR_TRC_SRGB);
    // No dithering: compare the transfer function itself.
    pl_render_params exact = pl_render_default_params;
    exact.dither_params = nullptr;
    const double pl_encode = median_ms(20, [&] {
        pl_render_image(renderer, &in, &out, &exact);
        pl_gpu_finish(gpu);
    });
    std::vector<std::uint8_t> theirs(1920 * 1080 * 4);
    pl_tex_transfer_params tp{};
    tp.tex = dst;
    tp.ptr = theirs.data();
    pl_tex_download(gpu, &tp);
    int worst = 0;
    std::size_t differ = 0;
    // Both against the exact IEC 61966-2-1 formula applied to the linear output (opaque).
    std::array<int, 2> worst_vs_formula{};
    const auto linear = (*comp)->read_output();
    if (ours && linear) {
        for (std::size_t i = 0; i < theirs.size(); ++i) {
            const int d = std::abs(int(theirs[i]) - int((*ours)[i]));
            worst = std::max(worst, d);
            differ += d != 0 ? 1 : 0;
            if (i % 4 == 3) {
                continue;
            }
            const double v = std::clamp(double(linear->pixels[i]), 0.0, 1.0);
            const double e = v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055;
            const int level = static_cast<int>(std::lround(e * 255.0));
            worst_vs_formula[0] = std::max(worst_vs_formula[0], std::abs(int((*ours)[i]) - level));
            worst_vs_formula[1] = std::max(worst_vs_formula[1], std::abs(int(theirs[i]) - level));
        }
    }
    std::printf("== 2. SDR display encode of the 1920x1080 RGBA16F compositor output (median of 20)\n"
                "  compositor encode_display  %6.2f ms\n  libplacebo (no dither)     %6.2f ms\n"
                "  max difference %d of 255, %zu of %zu samples differ\n",
                ours_encode, pl_encode, worst, differ, theirs.size());
    std::printf("  vs the sRGB formula: compositor %d, libplacebo %d levels at most\n",
                worst_vs_formula[0], worst_vs_formula[1]);
    pl_tex_destroy(gpu, &src); // the wrapper only; the compositor still owns the image
    pl_tex_destroy(gpu, &dst);

    // 3. YUV->RGB + downscale from a VA-API frame on our device.
    auto access = (*hw_frame)->acquire_gpu();
    if (!access) {
        std::fprintf(stderr, "acquire_gpu failed\n");
        return 1;
    }
    const media::GpuImages& im = access->images();
    const auto layout = (*hw_frame)->layout();
    std::printf("== 3. VA-API frame: %d image(s), formats %d/%d\n", im.image_count,
                static_cast<int>(im.formats[0]), static_cast<int>(im.formats[1]));
    const int w = (*hw_frame)->width();
    const int h = (*hw_frame)->height();
    std::array<pl_tex, 2> planes{};
    for (int p = 0; p < 2 && std::cmp_less(p, im.image_count); ++p) {
        const int sx = p == 0 ? 0 : layout.chroma_shift_x;
        const int sy = p == 0 ? 0 : layout.chroma_shift_y;
        planes[p] = wrap(gpu, im.images[p], w >> sx, h >> sy, im.formats[p],
                         VK_IMAGE_USAGE_SAMPLED_BIT);
        if (planes[p] == nullptr) {
            std::printf("  wrap of plane %d FAILED\n", p);
            return 1;
        }
        release(gpu, planes[p], im.layouts[p], {im.semaphores[p], im.wait_values[p]});
    }
    if (planes[0] == nullptr || planes[1] == nullptr) {
        std::printf("  wrap FAILED (expected two plane images from VA-API)\n");
        return 1;
    }
    pl_frame yuv{};
    yuv.num_planes = 2;
    yuv.planes[0].texture = planes[0];
    yuv.planes[0].components = 1;
    yuv.planes[0].component_mapping[0] = 0;
    yuv.planes[1].texture = planes[1];
    yuv.planes[1].components = 2;
    yuv.planes[1].component_mapping[0] = 1;
    yuv.planes[1].component_mapping[1] = 2;
    yuv.repr.sys = PL_COLOR_SYSTEM_BT_709;
    yuv.repr.levels = PL_COLOR_LEVELS_LIMITED;
    yuv.repr.bits.sample_depth = 8;
    yuv.repr.bits.color_depth = 8;
    yuv.color = pl_color_space_bt709;
    pl_chroma_location_offset(PL_CHROMA_LEFT, &yuv.planes[1].shift_x, &yuv.planes[1].shift_y);
    pl_tex small = make_tex(gpu, 1280, 720, "rgba16f", false);
    pl_frame target = rgb_frame(small, PL_COLOR_TRC_LINEAR);
    std::printf("  YUV->RGB + 1920x1080 -> 1280x720 linear RGBA16F (median of 20)\n");
    const std::array<std::pair<const char*, const pl_render_params*>, 3> presets{
        {{"libplacebo fast", &pl_render_fast_params},
         {"libplacebo default", &pl_render_default_params},
         {"libplacebo high quality", &pl_render_high_quality_params}}};
    for (const auto& [name, params] : presets) {
        bool ok = true;
        const double ms = median_ms(20, [&] {
            ok = ok && pl_render_image(renderer, &yuv, &target, params);
            pl_gpu_finish(gpu);
        });
        std::printf("  %-26s %6.2f ms%s\n", name, ms, ok ? "" : " (render FAILED)");
    }
    // Hand the planes back to FFmpeg's contract: signal its semaphores, then commit.
    for (int p = 0; p < 2; ++p) {
        pl_vulkan_hold_params hp{};
        hp.tex = planes[p];
        hp.layout = VK_IMAGE_LAYOUT_GENERAL;
        hp.qf = VK_QUEUE_FAMILY_IGNORED;
        hp.semaphore = {im.semaphores[p], im.signal_values[p]};
        pl_vulkan_hold_ex(gpu, &hp);
    }
    access->commit(VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    { const media::GpuAccess done = std::move(*access); } // release FFmpeg's frame lock
    pl_gpu_finish(gpu);

    compositor::RenderGraph small_graph;
    small_graph.width = 1280;
    small_graph.height = 720;
    small_graph.layers.push_back(compositor::Layer{});
    const std::array<compositor::LayerInput, 1> hw_input{
        compositor::LayerInput{.frame = &**hw_frame, .color = color}};
    const double ours_scale =
        median_ms(20, [&] { static_cast<void>((*comp)->render(small_graph, hw_input)); });
    std::printf("  %-26s %6.2f ms (bilinear, includes its own output clear)\n", "compositor",
                ours_scale);

    for (auto& p : planes) {
        pl_tex_destroy(gpu, &p);
    }
    pl_tex_destroy(gpu, &small);
    pl_renderer_destroy(&renderer);
    pl_vulkan_destroy(&vk);
    pl_log_destroy(&log);
    return 0;
}
