#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"

#include "compositor_test.hpp"
#include "nv12_reference.hpp"

#include <array>
#include <cstdio>

#include "oma_test.hpp"

// read_nv12 (export's hardware-encoder input) against a CPU reference (nv12_reference.hpp).

namespace oc = oma::compositor;

namespace {

void matches_cpu_conversion() {
    const oma::gpu::Device* device = compositor_test_device();
    auto src = decode_first("h264_30fps_aac.mp4");
    if (device == nullptr || !src.frame)
        return;
    auto vk = oc::VulkanCompositor::create(*device);
    oc::RenderGraph g;
    g.width = 318; // even, not a multiple of 4: the row padding path
    g.height = 180;
    g.layers.resize(1);
    const std::array<oc::LayerInput, 1> in{src.input()};
    expect(vk && (*vk)->render(g, in).has_value()).toBeTruthy();
    const auto nv12 = (*vk)->read_nv12();
    const bool encoded = (*vk)->encode_display(oc::VulkanCompositor::Transfer::Bt709).has_value();
    const auto rgba = (*vk)->read_display();
    expect(nv12 && encoded && rgba).toBeTruthy();
    if (!nv12 || !rgba)
        return;
    expect(static_cast<int>(nv12->stride)).toBe(320);
    const auto worst = nv12_reference::nv12_vs_rgba(*nv12, *rgba);
    std::printf("    NV12 vs CPU from RGBA8: luma %d, chroma %d levels\n", worst[0], worst[1]);
    expect(worst[0] <= 1 && worst[1] <= 2).toBeTruthy();
    g.width = 319;
    expect((*vk)->render(g, in).has_value() && !(*vk)->read_nv12().has_value()).toBeTruthy();
}

} // namespace

void run_nv12_tests() {
    describe("compositor::VulkanCompositor::read_nv12",
             { it("matches a CPU conversion of the same render", { matches_cpu_conversion(); }); });
}
