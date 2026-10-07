#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_frame.hpp"

#include "compositor_test.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>
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

// busy_graph with every look stage active: color adjustments on the base, a vignette on the
// picture-in-picture, vintage on the overlay.
RenderGraph looked_graph(std::uint32_t w, std::uint32_t h) {
    RenderGraph g = busy_graph(w, h);
    g.layers[0].color = {.exposure = 0.5, .contrast = 0.3, .saturation = -0.4, .temperature = 0.6};
    g.layers[1].looks.push_back({.kind = oma::compositor::FilterKind::Vignette, .amount = 0.8});
    g.layers[1].color.contrast = -0.5;
    g.layers[2].looks.push_back({.kind = oma::compositor::FilterKind::Vintage, .amount = 0.7});
    g.layers[2].looks.push_back({.kind = oma::compositor::FilterKind::Cool, .amount = 0.5});
    g.layers[0].sharpness = -0.4; // blurred background
    g.layers[2].sharpness = 0.8;  // sharpened overlay
    g.layers[1].reveal = 0.6;     // a wipe edge through the picture-in-picture
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

// Renders the native test layer with a look and returns the pixel at (x, y), straight alpha.
std::array<float, 4> looked(const LayerInput& input, const oma::compositor::ColorAdjust& color,
                            const oma::compositor::Filter& filter, std::uint32_t x,
                            std::uint32_t y) {
    RenderGraph g = native_graph(320, 180);
    g.layers[0].color = color;
    if (filter.kind != oma::compositor::FilterKind::None) {
        g.layers[0].looks.push_back(filter);
    }
    const std::array<LayerInput, 1> inputs{input};
    const auto out = CpuCompositor{}.render(g, inputs);
    return out ? out->at(x, y) : std::array<float, 4>{};
}

double luma(const std::array<float, 4>& p) {
    return (0.2126 * p[0]) + (0.7152 * p[1]) + (0.0722 * p[2]);
}

void color_adjustments() {
    // Flat patches, 80 px wide: dark neutral grey, mid neutral grey, vivid orange, white.
    auto src = decode_first("color_patches.y4m");
    if (!src.frame) {
        std::printf("    (skipped: fixture color_patches.y4m missing)\n");
        return;
    }
    using oma::compositor::ColorAdjust;
    using oma::compositor::Filter;
    using oma::compositor::FilterKind;
    const LayerInput in = src.input();
    const auto at = [&](std::uint32_t x, const ColorAdjust& c, const Filter& f) {
        return looked(in, c, f, x, 90);
    };
    const std::uint32_t dark = 40;
    const std::uint32_t mid = 120;
    const std::uint32_t vivid = 200;

    // Exposure scales linear light: +1 stop doubles it (no clipping in the float working space).
    const auto base = at(mid, {}, {});
    ColorAdjust brighter;
    brighter.exposure = 1.0;
    const auto twice = at(mid, brighter, {});
    for (int c = 0; c < 3; ++c) {
        expect(static_cast<double>(twice[c])).toBeCloseTo(2.0 * static_cast<double>(base[c]), 1e-5);
    }
    // Temperature tints a neutral without changing its luminance.
    ColorAdjust warm;
    warm.temperature = 1.0;
    const auto w = at(mid, warm, {});
    expect(w[0] > base[0] && w[2] < base[2]).toBeTruthy();
    expect(luma(w)).toBeCloseTo(luma(base), 1e-4);
    // Contrast pivots on 18% grey: below it darker, white brighter.
    ColorAdjust punchy;
    punchy.contrast = 0.5;
    expect(luma(at(dark, punchy, {})) < luma(at(dark, {}, {}))).toBeTruthy();
    expect(luma(at(280, punchy, {})) > luma(at(280, {}, {}))).toBeTruthy();

    // Saturation -1 and black & white both give the luminance on every channel.
    const auto color = at(vivid, {}, {});
    ColorAdjust grey;
    grey.saturation = -1.0;
    const auto g = at(vivid, grey, {});
    expect(std::abs(g[0] - g[1]) < 1e-6F && std::abs(g[1] - g[2]) < 1e-6F).toBeTruthy();
    expect(static_cast<double>(g[0])).toBeCloseTo(luma(color), 1e-5);
    const auto bw = at(vivid, {}, Filter{.kind = FilterKind::BlackAndWhite, .amount = 1.0});
    expect(static_cast<double>(bw[0])).toBeCloseTo(static_cast<double>(g[0]), 1e-6);
    // Half a sepia is halfway between the original and the full look.
    const auto sepia = at(vivid, {}, Filter{.kind = FilterKind::Sepia, .amount = 1.0});
    const auto half = at(vivid, {}, Filter{.kind = FilterKind::Sepia, .amount = 0.5});
    expect(static_cast<double>(half[0]))
        .toBeCloseTo((static_cast<double>(color[0]) + static_cast<double>(sepia[0])) / 2.0, 1e-5);

    // Looks apply in order (ADR-0016): black and white last leaves grey, sepia last tints it.
    RenderGraph stack = native_graph(320, 180);
    const Filter to_bw{.kind = FilterKind::BlackAndWhite, .amount = 1.0};
    const Filter to_sepia{.kind = FilterKind::Sepia, .amount = 1.0};
    stack.layers[0].looks = {to_sepia, to_bw};
    const std::array<LayerInput, 1> inputs{in};
    const auto grey_last = CpuCompositor{}.render(stack, inputs);
    stack.layers[0].looks = {to_bw, to_sepia};
    const auto sepia_last = CpuCompositor{}.render(stack, inputs);
    expect(grey_last.has_value() && sepia_last.has_value()).toBeTruthy();
    if (grey_last && sepia_last) {
        const auto a = grey_last->at(vivid, 90);
        const auto b = sepia_last->at(vivid, 90);
        expect(std::abs(a[0] - a[2]) < 1e-6F).toBeTruthy();
        expect(static_cast<double>(b[0])).toBeCloseTo(static_cast<double>(sepia[0]), 1e-5);
    }
}

// The native patches layer with a sharpness, pixel (x, 90).
std::array<float, 4> detailed_at(const LayerInput& input, double sharpness, std::uint32_t x) {
    RenderGraph g = native_graph(320, 180);
    g.layers[0].sharpness = sharpness;
    const std::array<LayerInput, 1> inputs{input};
    const auto out = CpuCompositor{}.render(g, inputs);
    return out ? out->at(x, 90) : std::array<float, 4>{};
}

void blur_and_sharpen() {
    auto src = decode_first("color_patches.y4m");
    if (!src.frame) {
        std::printf("    (skipped: fixture color_patches.y4m missing)\n");
        return;
    }
    const LayerInput in = src.input();
    // x = 78 is dark grey, 2 px left of the mid grey patch; x = 40 is far from any edge.
    const double edge = luma(detailed_at(in, 0.0, 78));
    const double flat = luma(detailed_at(in, 0.0, 40));
    // Blur spreads the brighter neighbour into the edge and leaves flat areas alone.
    expect(luma(detailed_at(in, -0.5, 78)) > edge + 0.005).toBeTruthy();
    expect(luma(detailed_at(in, -0.5, 40))).toBeCloseTo(flat, 1e-4);
    // Sharpen pushes the dark side of the edge darker (unsharp-mask undershoot), next to it.
    expect(luma(detailed_at(in, 1.0, 79)) < luma(detailed_at(in, 0.0, 79)) - 0.002).toBeTruthy();
    expect(luma(detailed_at(in, 1.0, 40))).toBeCloseTo(flat, 1e-4);
}

void reveal_shows_the_left_part() {
    auto src = decode_first("h264_30fps_aac.mp4");
    if (!src.frame) {
        return;
    }
    const std::array<LayerInput, 1> inputs{src.input()};
    RenderGraph g = native_graph(320, 180);
    g.background = {0.0F, 0.0F, 0.0F, 0.0F};
    g.layers[0].reveal = 0.25; // the edge at x = 80
    const auto out = CpuCompositor{}.render(g, inputs);
    expect(out.has_value()).toBeTruthy();
    if (!out) {
        return;
    }
    expect(static_cast<double>(out->at(79, 90)[3])).toBeCloseTo(1.0, 1e-6);
    expect(static_cast<double>(out->at(80, 90)[3])).toBeCloseTo(0.0, 1e-6);
    g.layers[0].reveal = 0.2515625; // 80.5 px: pixel 80 half covered
    const auto half = CpuCompositor{}.render(g, inputs);
    expect(half && std::abs(half->at(80, 90)[3] - 0.5F) < 1e-5F).toBeTruthy();
}

void rotated_edges_have_partial_coverage() {
    auto src = decode_first("still.png");
    if (!src.frame) {
        return;
    }
    const std::array<LayerInput, 1> inputs{src.input()};
    RenderGraph g = native_graph(160, 120);
    g.background = {0.0F, 0.0F, 0.0F, 0.0F};
    g.layers[0].transform = {.scale_x = 0.2, .scale_y = 0.2, .rotation = 23.0};
    const auto cpu = CpuCompositor{}.render(g, inputs);
    expect(cpu.has_value()).toBeTruthy();
    if (!cpu) {
        return;
    }
    std::size_t partial = 0;
    std::size_t opaque = 0;
    for (std::uint32_t y = 0; y < g.height; ++y) {
        for (std::uint32_t x = 0; x < g.width; ++x) {
            const float alpha = cpu->at(x, y)[3];
            partial += alpha > 0.0F && alpha < 1.0F ? 1U : 0U;
            opaque += alpha == 1.0F ? 1U : 0U;
        }
    }
    expect(partial > 0 && opaque > 0).toBeTruthy();

    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    expect(gpu.has_value()).toBeTruthy();
    if (gpu) {
        expect(compare(*gpu, *cpu, kTolerance).over_tolerance == 0U).toBeTruthy();
    }
}

void vignette_darkens_the_corners() {
    auto src = decode_first("h264_30fps_aac.mp4");
    if (!src.frame) {
        return;
    }
    using oma::compositor::Filter;
    using oma::compositor::FilterKind;
    const LayerInput in = src.input();
    const Filter vignette{.kind = FilterKind::Vignette, .amount = 1.0};
    const auto center = looked(in, {}, vignette, 160, 90);
    const auto corner = looked(in, {}, vignette, 2, 2);
    expect(center == looked(in, {}, {}, 160, 90)).toBeTruthy();
    expect(luma(corner) < 0.3 * luma(looked(in, {}, {}, 2, 2))).toBeTruthy();
}

void rejects_bad_inputs() {
    RenderGraph g = native_graph(16, 16);
    const std::array<LayerInput, 1> empty{LayerInput{}};
    expect(CpuCompositor{}.render(g, empty).has_value()).toBeFalsy();
    expect(CpuCompositor{}.render(g, {}).has_value()).toBeFalsy();
}

void gpu_matches_cpu_with_looks() {
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
    const RenderGraph g = looked_graph(640, 360);
    const auto cpu = CpuCompositor{}.render(g, inputs);
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    expect(cpu && gpu).toBeTruthy();
    if (!cpu || !gpu) {
        return;
    }
    const auto diff = compare(*gpu, *cpu, kTolerance);
    std::printf("    max difference %.5f, %zu of %zu pixels over tolerance\n",
                static_cast<double>(diff.max_abs), diff.over_tolerance, cpu->pixels.size() / 4);
    expect(diff.over_tolerance <= cpu->pixels.size() / 4 / 500).toBeTruthy();
}

// A strong blur on a 640x360 still: its sigma (7.2 px) reduces the source by 2 before the
// gaussian (src/look.hpp Detail), with a crop so the reduced grid starts off the origin.
void gpu_matches_cpu_with_reduced_blur() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto still = decode_first("still.png");
    if (!still.frame) {
        std::printf("    (skipped: fixture still.png missing)\n");
        return;
    }
    const std::array<LayerInput, 1> inputs{still.input()};
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    RenderGraph g = native_graph(640, 360);
    g.layers[0].sharpness = -1.0;
    g.layers[0].crop = {.left = 0.05, .top = 0.1, .right = 0.0, .bottom = 0.0};
    const auto cpu = CpuCompositor{}.render(g, inputs);
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    expect(cpu && gpu).toBeTruthy();
    if (!cpu || !gpu) {
        return;
    }
    const auto diff = compare(*gpu, *cpu, kTolerance);
    std::printf("    max difference %.5f, %zu of %zu pixels over tolerance\n",
                static_cast<double>(diff.max_abs), diff.over_tolerance, cpu->pixels.size() / 4);
    expect(diff.over_tolerance <= cpu->pixels.size() / 4 / 500).toBeTruthy();
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

// A GBRA source (ADR-0015 titles): its straight alpha scales the layer's coverage in both
// compositors. White text over black: opaque, half and fully transparent columns.
void straight_alpha_layers() {
    constexpr int kW = 48;
    constexpr int kH = 16;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kW) * kH * 4, 255);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            rgba[((static_cast<std::size_t>(y) * kW) + x) * 4 + 3] = x < 16   ? 255
                                                                     : x < 32 ? 128
                                                                              : 0;
        }
    }
    auto frame = oma::media::VideoFrame::from_rgba(kW, kH, rgba, kW * 4);
    expect(frame.has_value()).toBeTruthy();
    if (!frame) {
        return;
    }
    expect(frame->layout().alpha && !frame->layout().yuv && frame->layout().planes == 4)
        .toBeTruthy();
    LayerInput input;
    input.frame = &*frame;
    const std::array<LayerInput, 1> inputs{input};
    RenderGraph g = native_graph(kW, kH);
    g.background = {0.0F, 0.0F, 0.0F, 1.0F};
    const auto cpu = CpuCompositor{}.render(g, inputs);
    expect(cpu.has_value()).toBeTruthy();
    if (!cpu) {
        std::printf("    %s\n", cpu.error().summary().c_str());
        return;
    }
    const auto near = [](float a, double b) {
        return std::abs(static_cast<double>(a) - b) < 1e-3;
    };
    expect(near(cpu->at(8, 8)[0], 1.0)).toBeTruthy();
    expect(near(cpu->at(24, 8)[0], 128.0 / 255.0)).toBeTruthy();
    expect(near(cpu->at(40, 8)[0], 0.0)).toBeTruthy();
    expect(near(cpu->at(40, 8)[3], 1.0)).toBeTruthy(); // the opaque background shows through

    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    expect(gpu.has_value()).toBeTruthy();
    if (gpu) {
        const auto diff = compare(*gpu, *cpu, kTolerance);
        std::printf("    max difference from the CPU reference %.5f\n",
                    static_cast<double>(diff.max_abs));
        expect(diff.over_tolerance).toBe(0U);
    }
}

// A PNG decodes to planar GBR; both compositors read it as sRGB, pixel for pixel.
void draws_rgb_images() {
    auto png = decode_first("still.png");
    if (!png.frame) {
        std::printf("    (skipped: fixture still.png missing)\n");
        return;
    }
    expect(png.frame->layout().yuv).toBeFalsy();
    const auto w = static_cast<std::uint32_t>(png.frame->width());
    const auto h = static_cast<std::uint32_t>(png.frame->height());
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4);
    expect(png.frame->copy_rgba(rgba, static_cast<int>(w) * 4).has_value()).toBeTruthy();
    const std::array<LayerInput, 1> inputs{png.input()};
    const RenderGraph g = native_graph(w, h);
    const auto cpu = CpuCompositor{}.render(g, inputs);
    expect(cpu.has_value()).toBeTruthy();
    if (!cpu) {
        return;
    }
    const auto linear = [](std::uint8_t v) {
        const double c = v / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    double worst = 0.0;
    for (std::uint32_t y = 0; y < h; y += 11) {
        for (std::uint32_t x = 0; x < w; x += 13) {
            const auto px = cpu->at(x, y);
            const std::size_t i = ((static_cast<std::size_t>(y) * w) + x) * 4;
            for (std::size_t c = 0; c < 3; ++c) {
                worst = std::max(worst, std::abs(static_cast<double>(px[c]) - linear(rgba[i + c])));
            }
        }
    }
    std::printf("    worst difference from the PNG %.6f\n", worst);
    expect(worst < 1e-4).toBeTruthy();

    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
    const auto gpu = (*vk)->read_output();
    expect(gpu.has_value()).toBeTruthy();
    if (gpu) {
        expect(compare(*gpu, *cpu, kTolerance).over_tolerance == 0U).toBeTruthy();
    }
}

// An independent decoded-frame check: FFmpeg's libswscale converts the same YUV frame to
// nonlinear RGB; the compositor should contain its BT.1886-decoded linear values. The Y4M
// patches are untagged (BT.601 fallback); the Matroska ones carry BT.709 at both ranges.
void draws_yuv_against_swscale(const char* fixture) {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr)
        return;
    auto src = decode_first(fixture);
    if (!src.frame) {
        std::printf("    (skipped: fixture %s missing)\n", fixture);
        return;
    }
    constexpr std::uint32_t w = 320;
    constexpr std::uint32_t h = 180;
    std::vector<std::uint8_t> rgba(w * h * 4);
    const auto converted = src.frame->copy_rgba(rgba, w * 4);
    expect(converted.has_value()).toBeTruthy();
    if (!converted)
        return;
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk)
        return;
    const std::array<LayerInput, 1> inputs{src.input()};
    expect((*vk)->render(native_graph(w, h), inputs).has_value()).toBeTruthy();
    const auto out = (*vk)->read_output();
    expect(out.has_value()).toBeTruthy();
    if (!out)
        return;
    double worst = 0.0;
    for (const std::uint32_t x : {40U, 120U, 200U, 280U}) {
        const auto pixel = out->at(x, h / 2);
        const std::size_t base = ((h / 2 * w) + x) * 4;
        for (std::size_t c = 0; c < 3; ++c) {
            const double expected = std::pow(static_cast<double>(rgba[base + c]) / 255.0, 2.4);
            worst = std::max(worst, std::abs(static_cast<double>(pixel[c]) - expected));
        }
    }
    std::printf("    %s: worst difference from libswscale %.5f\n", fixture, worst);
    expect(worst < 0.025).toBeTruthy();
}

// Independent chroma reconstruction for 8-bit 4:2:0 BT.601 limited range, written from the
// standards rather than the compositor: chroma samples sit at luma x = 2i (left) or 2i + 0.5
// (center) and halfway between luma rows, are interpolated bilinearly with clamped edges, then
// converted with the BT.601 equations and decoded with BT.1886. Returns linear red at (x, y).
double reference_red(const oma::media::VideoFrame& f, int x, int y, bool left_sited) {
    const auto sample = [&](int index, int cx, int cy) {
        cx = std::clamp(cx, 0, (f.width() / 2) - 1);
        cy = std::clamp(cy, 0, (f.height() / 2) - 1);
        return static_cast<double>(f.plane(
            index)[(static_cast<std::size_t>(cy) * static_cast<std::size_t>(f.stride(index))) +
                   static_cast<std::size_t>(cx)]);
    };
    const double px = ((x + 0.5) - (left_sited ? 0.5 : 1.0)) / 2.0;
    const double py = ((y + 0.5) - 1.0) / 2.0;
    const int x0 = static_cast<int>(std::floor(px));
    const int y0 = static_cast<int>(std::floor(py));
    const double fx = px - x0;
    const double fy = py - y0;
    const auto bilinear = [&](int index) {
        const double top = (sample(index, x0, y0) * (1 - fx)) + (sample(index, x0 + 1, y0) * fx);
        const double bottom =
            (sample(index, x0, y0 + 1) * (1 - fx)) + (sample(index, x0 + 1, y0 + 1) * fx);
        return (top * (1 - fy)) + (bottom * fy);
    };
    const double luma = static_cast<double>(
        f.plane(0)[(static_cast<std::size_t>(y) * static_cast<std::size_t>(f.stride(0))) +
                   static_cast<std::size_t>(x)]);
    const double yn = (luma - 16.0) / 219.0;
    const double pr = (bilinear(2) - 128.0) / 224.0;
    const double red = std::clamp(yn + (1.402 * pr), 0.0, 1.0);
    return std::pow(red, 2.4);
}

void chroma_location_changes_color_edges() {
    auto src = decode_first("color_patches.y4m");
    if (!src.frame) {
        return;
    }
    auto center = src.input();
    center.color.chroma_location = 2;
    // BT.709 primaries keep the gamut conversion out of the siting reference below.
    center.color.primaries = 1;
    auto left = center;
    left.color.chroma_location = 1;
    const RenderGraph g = native_graph(320, 180);
    const std::array<LayerInput, 1> centered{center};
    const std::array<LayerInput, 1> left_sited{left};
    const auto a = CpuCompositor{}.render(g, centered);
    const auto b = CpuCompositor{}.render(g, left_sited);
    expect(a && b).toBeTruthy();
    if (!a || !b) {
        return;
    }
    // The vivid patch starts at x=160; siting changes the interpolation at its edge.
    expect(std::abs(a->at(159, 90)[0] - b->at(159, 90)[0]) > 0.002F).toBeTruthy();
    expect(std::abs(a->at(120, 90)[0] - b->at(120, 90)[0]) < 1e-5F).toBeTruthy();
    // Both sitings against the independent reconstruction across the edge (the decoded
    // samples, so the shared chroma positions and interpolation are what is compared).
    double worst = 0.0;
    for (int x = 150; x <= 170; ++x) {
        worst = std::max(worst, std::abs(a->at(static_cast<std::uint32_t>(x), 90)[0] -
                                         reference_red(*src.frame, x, 90, false)));
        worst = std::max(worst, std::abs(b->at(static_cast<std::uint32_t>(x), 90)[0] -
                                         reference_red(*src.frame, x, 90, true)));
    }
    std::printf("    worst difference from the reference chroma reconstruction %.5f\n", worst);
    expect(worst < 0.002).toBeTruthy();

    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    for (const auto& [input, expected] : {std::pair{centered, &*a}, std::pair{left_sited, &*b}}) {
        expect((*vk)->render(g, input).has_value()).toBeTruthy();
        const auto gpu = (*vk)->read_output();
        expect(gpu.has_value()).toBeTruthy();
        if (gpu) {
            expect(compare(*gpu, *expected, kTolerance).over_tolerance == 0U).toBeTruthy();
        }
    }
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
    // The export transfer: BT.709 OETF (ITU-R BT.709-6 1.2), within one 8-bit level too.
    expect((*vk)->encode_display(oma::compositor::VulkanCompositor::Transfer::Bt709).has_value())
        .toBeTruthy();
    const auto video = (*vk)->read_display();
    expect(video.has_value()).toBeTruthy();
    if (!video) {
        return;
    }
    int worst709 = 0;
    for (std::size_t i = 0; i < video->size(); ++i) {
        if (i % 4 == 3) {
            continue;
        }
        const double l = std::clamp(static_cast<double>(linear->pixels[i]), 0.0, 1.0);
        const double v = l < 0.018 ? 4.5 * l : (1.099 * std::pow(l, 0.45)) - 0.099;
        worst709 = std::max(worst709, std::abs(static_cast<int>((*video)[i]) -
                                               static_cast<int>(std::lround(v * 255.0))));
    }
    std::printf("    BT.709 worst difference %d of 255\n", worst709);
    expect(worst709 <= 1).toBeTruthy();
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
        it("adjusts exposure, saturation, temperature, contrast and filters",
           { color_adjustments(); });
        it("darkens the corners with a vignette", { vignette_darkens_the_corners(); });
        it("blurs and sharpens edges, leaving flat areas", { blur_and_sharpen(); });
        it("uses declared chroma siting at color edges",
           { chroma_location_changes_color_edges(); });
        it("reveals the left part of a layer for wipes", { reveal_shows_the_left_part(); });
        it("anti-aliases rotated layer edges", { rotated_edges_have_partial_coverage(); });
    });
}

void run_vulkan_compositor_tests() {
    describe("compositor::VulkanCompositor", {
        it("matches the CPU reference on uploaded frames", { gpu_matches_cpu_with_uploads(); });
        it("matches the CPU reference with color adjustments and filters",
           { gpu_matches_cpu_with_looks(); });
        it("matches the CPU reference with a reduced blur",
           { gpu_matches_cpu_with_reduced_blur(); });
        it("matches the CPU reference on zero-copy GPU frames",
           { gpu_frames_match_cpu_reference(); });
        it("encodes the output for an SDR display", { display_encodes_srgb(); });
        it("draws RGB images as sRGB", { draws_rgb_images(); });
        it("blends sources with straight alpha like the CPU reference",
           { straight_alpha_layers(); });
        it("matches libswscale on decoded YUV patches", {
            draws_yuv_against_swscale("color_patches.y4m");
            draws_yuv_against_swscale("color_patches_bt709_tv.mkv");
            draws_yuv_against_swscale("color_patches_bt709_pc.mkv");
        });
        it("measures three 1080p layers", { measures_1080p_three_layers(); });
    });
}
