#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_frame.hpp"

#include "compositor_test.hpp"

#include <array>
#include <cstdio>

#include "oma_test.hpp"

// Regression (2026-10-14, cross-editor bench): two layers over one hardware-decoded frame made
// the compositor acquire the frame twice on one thread, which waited on its own lock forever.

namespace oc = oma::compositor;

namespace {

void two_layers_over_one_gpu_frame() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto hw = decode_first("h264_30fps_aac.mp4", device);
    auto sw = decode_first("h264_30fps_aac.mp4");
    if (!hw.frame || !sw.frame || !hw.frame->on_gpu()) {
        std::printf("    (skipped: no hardware decode here)\n");
        return;
    }
    auto vk = oc::VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    oc::RenderGraph g;
    g.width = 320;
    g.height = 180;
    g.layers.resize(2);
    g.layers[1].input = 1; // a second input, the same frame: a clip stacked over itself
    g.layers[1].transform.scale_x = 0.5;
    g.layers[1].transform.scale_y = 0.5;
    const std::array<oc::LayerInput, 2> gpu_inputs{hw.input(), hw.input()};
    const std::array<oc::LayerInput, 2> cpu_inputs{sw.input(), sw.input()};
    expect((*vk)->render(g, gpu_inputs).has_value()).toBeTruthy();
    expect((*vk)->render(g, gpu_inputs).has_value()).toBeTruthy(); // the lock was released
    const auto gpu = (*vk)->read_output();
    const auto cpu = oc::CpuCompositor{}.render(g, cpu_inputs);
    expect(cpu && gpu).toBeTruthy();
    if (cpu && gpu)
        expect(compare(*gpu, *cpu, 4e-3F).over_tolerance <= cpu->pixels.size() / 4 / 500)
            .toBeTruthy();
}

} // namespace

void run_shared_frame_tests() {
    describe("compositor::VulkanCompositor shared frames", {
        it("renders two layers over one GPU frame", { two_layers_over_one_gpu_frame(); });
    });
}
