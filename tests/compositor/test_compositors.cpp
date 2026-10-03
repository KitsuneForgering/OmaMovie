#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_frame.hpp"

#include "compositor_test.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "oma_test.hpp"

using oma::compositor::BlendMode;
using oma::compositor::CpuCompositor;
using oma::compositor::Fit;
using oma::compositor::Layer;
using oma::compositor::LayerInput;
using oma::compositor::RenderGraph;
using oma::compositor::RgbaImage;
using oma::compositor::VulkanCompositor;

namespace {

constexpr float kTolerance = 4e-3F; // half-float output and float vs float rounding

RenderGraph native_graph(std::uint32_t w, std::uint32_t h) {
    RenderGraph g;
    g.width = w;
    g.height = h;
    Layer l;
    l.fit = Fit::Native;
    g.layers.push_back(l);
    return g;
}

// Three layers exercising every stage: a fitted background, a cropped, scaled, rotated and
// translucent picture-in-picture, and a screen-blended overlay from a second source.
RenderGraph busy_graph(std::uint32_t w, std::uint32_t h) {
    RenderGraph g;
    g.width = w;
    g.height = h;
    g.background = {0.1F, 0.2F, 0.3F, 1.0F};
    Layer base;
    base.fit = Fit::Fit;
    Layer pip;
    pip.input = 1;
    pip.crop = {.left = 0.1, .top = 0.05, .right = 0.2, .bottom = 0.15};
    pip.transform = {.offset_x = static_cast<double>(w) / 5.0,
                     .offset_y = -static_cast<double>(h) / 6.0,
                     .scale_x = 0.45,
                     .scale_y = 0.45,
                     .rotation = 12.5};
    pip.opacity = 0.7F;
    Layer overlay;
    overlay.input = 1;
    overlay.fit = Fit::Fill;
    overlay.blend = BlendMode::Screen;
    overlay.opacity = 0.35F;
    g.layers = {base, pip, overlay};
    return g;
}

void background_only() {
    RenderGraph g;
    g.width = 8;
    g.height = 4;
    g.background = {1.0F, 0.5F, 0.25F, 0.5F};
    const auto out = CpuCompositor{}.render(g, {});
    expect(out.has_value()).toBeTruthy();
    if (out) {
        const auto px = out->at(7, 3);
        // Premultiplied.
        expect(static_cast<double>(px[0])).toBeCloseTo(0.5, 1e-6);
        expect(static_cast<double>(px[1])).toBeCloseTo(0.25, 1e-6);
        expect(static_cast<double>(px[3])).toBeCloseTo(0.5, 1e-6);
    }
}

void native_layer_is_opaque_and_shifts_exactly() {
    auto src = decode_first("h264_30fps_aac.mp4");
    expect(src.frame.has_value()).toBeTruthy();
    if (!src.frame) {
        return;
    }
    const std::array<LayerInput, 1> inputs{src.input()};
    RenderGraph g = native_graph(320, 180);
    g.background = {1.0F, 0.0F, 1.0F, 1.0F};
    const auto a = CpuCompositor{}.render(g, inputs);
    g.layers[0].transform.offset_x = 10;
    g.layers[0].transform.offset_y = 4;
    const auto b = CpuCompositor{}.render(g, inputs);
    expect(a && b).toBeTruthy();
    if (!a || !b) {
        return;
    }
    // Opaque everywhere; translating by whole pixels moves pixels exactly.
    expect(static_cast<double>(a->at(0, 0)[3])).toBeCloseTo(1.0, 1e-6);
    bool shifted = true;
    for (std::uint32_t y = 4; y < 180; y += 7) {
        for (std::uint32_t x = 10; x < 320; x += 13) {
            shifted = shifted && b->at(x, y) == a->at(x - 10, y - 4);
        }
    }
    expect(shifted).toBeTruthy();
    // Uncovered pixels keep the background.
    expect(static_cast<double>(b->at(5, 100)[1])).toBeCloseTo(0.0, 1e-6);
    expect(static_cast<double>(b->at(5, 100)[2])).toBeCloseTo(1.0, 1e-6);
}

void opacity_and_blend_modes() {
    auto src = decode_first("h264_30fps_aac.mp4");
    if (!src.frame) {
        return;
    }
    const std::array<LayerInput, 1> inputs{src.input()};
    RenderGraph g = native_graph(320, 180);
    g.background = {0.0F, 0.0F, 0.0F, 0.0F};
    const auto full = CpuCompositor{}.render(g, inputs);
    g.layers[0].opacity = 0.5F;
    const auto half = CpuCompositor{}.render(g, inputs);
    g.layers[0].opacity = 1.0F;
    Layer added = g.layers[0];
    added.blend = BlendMode::Add;
    g.layers.push_back(added);
    const auto doubled = CpuCompositor{}.render(g, inputs);
    expect(full && half && doubled).toBeTruthy();
    if (!full || !half || !doubled) {
        return;
    }
    const auto f = full->at(100, 60);
    const auto h = half->at(100, 60);
    const auto d = doubled->at(100, 60);
    expect(static_cast<double>(h[0])).toBeCloseTo(static_cast<double>(f[0]) / 2.0, 1e-6);
    expect(static_cast<double>(h[3])).toBeCloseTo(0.5, 1e-6);
    expect(static_cast<double>(d[1])).toBeCloseTo(static_cast<double>(f[1]) * 2.0, 1e-6);
    expect(static_cast<double>(d[3])).toBeCloseTo(1.0, 1e-6);
}

void rejects_bad_inputs() {
    RenderGraph g = native_graph(16, 16);
    const std::array<LayerInput, 1> empty{LayerInput{}};
    expect(CpuCompositor{}.render(g, empty).has_value()).toBeFalsy();
    expect(CpuCompositor{}.render(g, {}).has_value()).toBeFalsy();
}

void gpu_matches_cpu_with_uploads() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto a = decode_first("h264_30fps_aac.mp4");
    auto b = decode_first("hevc_10bit.mp4");
    if (!a.frame || !b.frame) {
        std::printf("    (skipped: fixtures missing)\n");
        return;
    }
    const std::array<LayerInput, 2> inputs{a.input(), b.input()};
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        std::printf("    %s\n", vk.error().summary().c_str());
        return;
    }
    const RenderGraph g = busy_graph(640, 360);
    const auto cpu = CpuCompositor{}.render(g, inputs);
    const auto rendered = (*vk)->render(g, inputs);
    expect(rendered.has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    expect(cpu && gpu).toBeTruthy();
    if (!cpu || !gpu) {
        return;
    }
    const auto diff = compare(*gpu, *cpu, kTolerance);
    std::printf("    max difference %.5f, %zu of %zu pixels over tolerance\n",
                static_cast<double>(diff.max_abs), diff.over_tolerance, cpu->pixels.size() / 4);
    // Coordinates exactly on a crop edge may round to different sides in float.
    expect(diff.over_tolerance <= cpu->pixels.size() / 4 / 500).toBeTruthy();
}

// The sRGB transfer function (IEC 61966-2-1), as the display pass applies it.
int srgb_level(float linear) {
    const double x = std::clamp(static_cast<double>(linear), 0.0, 1.0);
    const double v = x <= 0.0031308 ? 12.92 * x : (1.055 * std::pow(x, 1.0 / 2.4)) - 0.055;
    return static_cast<int>(std::lround(v * 255.0));
}

// The display pass encodes the linear output with the sRGB curve, within one 8-bit level, and
// keeps alpha.
void display_encodes_srgb() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto a = decode_first("h264_30fps_aac.mp4");
    auto b = decode_first("hevc_10bit.mp4");
    if (!a.frame || !b.frame) {
        std::printf("    (skipped: fixtures missing)\n");
        return;
    }
    const std::array<LayerInput, 2> inputs{a.input(), b.input()};
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    const RenderGraph g = busy_graph(320, 180);
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
    const auto encoded = (*vk)->encode_display();
    expect(encoded.has_value()).toBeTruthy();
    const auto linear = (*vk)->read_output();
    const auto display = (*vk)->read_display();
    expect(linear && display).toBeTruthy();
    if (!linear || !display) {
        return;
    }
    expect(display->size() == linear->pixels.size()).toBeTruthy();
    int worst = 0;
    for (std::size_t i = 0; i < display->size(); ++i) {
        const int expected =
            i % 4 == 3
                ? static_cast<int>(std::lround(std::clamp(linear->pixels[i], 0.0F, 1.0F) * 255.0F))
                : srgb_level(linear->pixels[i]);
        worst = std::max(worst, std::abs(static_cast<int>((*display)[i]) - expected));
    }
    std::printf("    worst difference %d of 255\n", worst);
    expect(worst <= 1).toBeTruthy();
}

void gpu_frames_match_cpu_reference() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto hw = decode_first("h264_30fps_aac.mp4", device);
    auto sw = decode_first("h264_30fps_aac.mp4");
    if (!hw.frame || !sw.frame) {
        return;
    }
    if (!hw.frame->on_gpu()) {
        std::printf("    (skipped: no hardware decode here)\n");
        return;
    }
    auto vk = VulkanCompositor::create(*device);
    if (!vk) {
        return;
    }
    RenderGraph g = busy_graph(640, 360);
    g.layers.resize(2);
    g.layers[1].input = 0;
    const std::array<LayerInput, 1> gpu_inputs{hw.input()};
    const std::array<LayerInput, 1> cpu_inputs{sw.input()};
    expect((*vk)->render(g, gpu_inputs).has_value()).toBeTruthy();
    // Render twice: the frame's semaphore and layout must carry over between submissions.
    expect((*vk)->render(g, gpu_inputs).has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    const auto cpu = CpuCompositor{}.render(g, cpu_inputs);
    expect(cpu && gpu).toBeTruthy();
    if (!cpu || !gpu) {
        return;
    }
    const auto diff = compare(*gpu, *cpu, kTolerance);
    std::printf("    %s frames: max difference %.5f, %zu pixels over tolerance\n",
                std::string(oma::media::to_string(hw.frame->path())).c_str(),
                static_cast<double>(diff.max_abs), diff.over_tolerance);
    expect(diff.over_tolerance <= cpu->pixels.size() / 4 / 500).toBeTruthy();
}

// Frame time for three 1080p layers (M3 "done when"); printed, not asserted.
void measures_1080p_three_layers() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto a = decode_first("h264_30fps_aac.mp4", device);
    auto b = decode_first("hevc_10bit.mp4", device);
    if (!a.frame || !b.frame) {
        return;
    }
    auto vk = VulkanCompositor::create(*device);
    if (!vk) {
        return;
    }
    const std::array<LayerInput, 2> inputs{a.input(), b.input()};
    const RenderGraph g = busy_graph(1920, 1080);
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy(); // warm-up
    constexpr int kRuns = 20;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kRuns; ++i) {
        static_cast<void>((*vk)->render(g, inputs));
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() /
        kRuns;
    std::printf("    1080p, 3 layers (%s inputs): %.2f ms per frame, submit to completion\n",
                a.frame->on_gpu() ? "GPU" : "uploaded", ms);
}

} // namespace

void run_cpu_compositor_tests() {
    describe("compositor::CpuCompositor", {
        it("fills the background premultiplied", { background_only(); });
        it("draws opaque layers and moves them by exact pixels",
           { native_layer_is_opaque_and_shifts_exactly(); });
        it("applies opacity and blend modes", { opacity_and_blend_modes(); });
        it("rejects missing inputs", { rejects_bad_inputs(); });
    });
}

void run_vulkan_compositor_tests() {
    describe("compositor::VulkanCompositor", {
        it("matches the CPU reference on uploaded frames", { gpu_matches_cpu_with_uploads(); });
        it("matches the CPU reference on zero-copy GPU frames",
           { gpu_frames_match_cpu_reference(); });
        it("encodes the output for an SDR display", { display_encodes_srgb(); });
        it("measures three 1080p layers", { measures_1080p_three_layers(); });
    });
}
