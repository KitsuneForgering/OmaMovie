// S5: two videos + one image through the library compositor, frame by frame, as playback would
// drive it. Usage: s5_minimal_compositing <video-a> <video-b> <image> [software|hardware] [frames]
// Decode and composite run sequentially on one thread, so the two times are separable; a real
// scheduler overlaps them. render() waits for the GPU, so composite time is submit to completion.
// No presentation. See Docs/spikes/S5-minimal-compositing.md.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_decoder.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>

using namespace oma;

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

void report(const char* name, std::vector<double> ms) {
    std::ranges::sort(ms);
    const auto at = [&](double q) { return ms[static_cast<std::size_t>(q * double(ms.size() - 1))]; };
    std::printf("  %-22s p50 %6.2f  p95 %6.2f  p99 %6.2f  max %6.2f ms\n", name, at(0.5), at(0.95),
                at(0.99), ms.back());
}

// Next frame, rewinding at the end so short files can feed long runs.
std::optional<media::VideoFrame> next_looping(media::VideoDecoder& d) {
    auto f = d.next();
    if (f && !*f && d.seek(RationalTime{})) {
        f = d.next();
    }
    if (!f || !*f) {
        return std::nullopt;
    }
    return std::move(**f);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <video-a> <video-b> <image> [software|hardware] [frames]\n",
                     argv[0]);
        return 2;
    }
    const bool hardware = argc > 4 && std::string_view(argv[4]) == "hardware";
    const int frames = argc > 5 ? std::atoi(argv[5]) : 600;

    auto device = gpu::Device::create();
    if (!device) {
        std::fprintf(stderr, "no Vulkan device: %s\n", device.error().summary().c_str());
        return 1;
    }
    media::VideoDecoderOptions options;
    options.device = device->get();
    if (!hardware) {
        options.paths = {media::DecodePath::Software};
    }
    std::array<std::unique_ptr<media::VideoDecoder>, 3> decoders;
    for (int i = 0; i < 3; ++i) {
        auto d = media::VideoDecoder::open(argv[i + 1], options);
        if (!d) {
            std::fprintf(stderr, "%s: %s\n", argv[i + 1], d.error().summary().c_str());
            return 1;
        }
        decoders[i] = std::move(*d);
    }
    auto image = next_looping(*decoders[2]); // decoded once, uploaded every render
    auto compositor = compositor::VulkanCompositor::create(**device);
    if (!image || !compositor) {
        std::fprintf(stderr, "image or compositor unavailable\n");
        return 1;
    }

    // The S5 scene: A fills the frame, B is a cropped, rotated, translucent picture in picture,
    // the image is a screen-blended overlay.
    compositor::RenderGraph g;
    g.width = 1920;
    g.height = 1080;
    compositor::Layer a;
    a.fit = compositor::Fit::Fill;
    compositor::Layer b;
    b.input = 1;
    b.crop = {.left = 0.1, .top = 0.05, .right = 0.2, .bottom = 0.15};
    b.transform = {.offset_x = 384, .offset_y = -180, .scale_x = 0.45, .scale_y = 0.45,
                   .rotation = 12.5};
    b.opacity = 0.7F;
    compositor::Layer overlay;
    overlay.input = 2;
    overlay.blend = compositor::BlendMode::Screen;
    overlay.opacity = 0.35F;
    g.layers = {a, b, overlay};

    std::printf("S5: %s + %s + %s -> 1920x1080, %d frames\n", argv[1], argv[2], argv[3], frames);
    for (int i = 0; i < 3; ++i) {
        const auto& v = decoders[i]->stream().video;
        std::printf("  input %d: %dx%d, path %s\n", i, v ? v->width : 0, v ? v->height : 0,
                    std::string(media::to_string(decoders[i]->path())).c_str());
    }

    std::vector<double> decode_ms;
    std::vector<double> render_ms;
    std::vector<double> total_ms;
    constexpr int kWarmup = 10;
    for (int i = 0; i < frames + kWarmup; ++i) {
        const auto t0 = Clock::now();
        auto fa = next_looping(*decoders[0]);
        auto fb = next_looping(*decoders[1]);
        const double dec = ms_since(t0);
        if (!fa || !fb) {
            std::fprintf(stderr, "decode failed at frame %d\n", i);
            return 1;
        }
        const std::array<compositor::LayerInput, 3> inputs{
            compositor::LayerInput{.frame = &*fa, .color = decoders[0]->stream().video->color},
            compositor::LayerInput{.frame = &*fb, .color = decoders[1]->stream().video->color},
            compositor::LayerInput{.frame = &*image, .color = decoders[2]->stream().video->color}};
        const auto t1 = Clock::now();
        if (auto r = (*compositor)->render(g, inputs); !r) {
            std::fprintf(stderr, "render failed: %s\n", r.error().summary().c_str());
            return 1;
        }
        const double ren = ms_since(t1);
        if (i >= kWarmup) {
            decode_ms.push_back(dec);
            render_ms.push_back(ren);
            total_ms.push_back(dec + ren);
        }
    }
    report("decode A + B", decode_ms);
    report("composite (3 layers)", render_ms);
    report("decode + composite", total_ms);
    const auto over = std::ranges::count_if(total_ms, [](double ms) { return ms > 1000.0 / 60.0; });
    std::printf("  frames over 16.67 ms: %td of %zu\n", over, total_ms.size());
    return 0;
}
