// M10 effects contract, memory per pass: the device memory each shipped effect adds to a warmed
// compositor, read from VK_EXT_memory_budget's heap usage (the driver's own count of this
// process's allocations). Looks run inside the composite pass and should add nothing; detail
// (blur/sharpen) keeps two RGBA16F images per layer, sized by its reduction factor.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"

#include "compositor_test.hpp"

#include <array>
#include <cstdio>
#include <utility>

#include "oma_test.hpp"

using oma::compositor::Fit;
using oma::compositor::Layer;
using oma::compositor::RenderGraph;
using oma::compositor::VulkanCompositor;

namespace {

VkDeviceSize heap_usage(const oma::gpu::Device& device) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
    budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    props.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(device.physical_device(), &props);
    VkDeviceSize used = 0;
    for (std::uint32_t i = 0; i < props.memoryProperties.memoryHeapCount; ++i)
        used += budget.heapUsage[i];
    return used;
}

// MiB a fresh compositor gains when `layer`'s effect is added to a warmed render of the source.
double added_mib(const oma::gpu::Device& device, DecodedFrame& source, std::uint32_t w,
                 std::uint32_t h, const Layer& layer) {
    auto vk = VulkanCompositor::create(device);
    if (!vk)
        return -1;
    const std::array inputs{source.input()};
    RenderGraph g;
    g.width = w;
    g.height = h;
    g.layers = {Layer{}};
    g.layers[0].fit = Fit::Fill;
    static_cast<void>((*vk)->render(g, inputs));
    const VkDeviceSize before = heap_usage(device);
    g.layers[0] = layer;
    g.layers[0].fit = Fit::Fill;
    static_cast<void>((*vk)->render(g, inputs));
    return static_cast<double>(heap_usage(device) - before) / (1 << 20);
}

} // namespace

// Looks add nothing measurable; detail stays within its two full-size RGBA16F images (16 bytes a
// source pixel at most; the 1080p source is the bound at both output sizes).
bool effects_stay_within_their_images() {
    const oma::gpu::Device* device = compositor_test_device();
    auto a = device ? decode_first("screenrecording_like.mp4", device) : DecodedFrame{};
    if (!a.frame)
        return true; // no Vulkan device or fixture: nothing to measure
    const double detail_bound = 1920.0 * 1080.0 * 16.0 / (1 << 20) + 1.0;
    bool ok = true;
    for (const auto& [w, h] : {std::pair{1920U, 1080U}, std::pair{3840U, 2160U}}) {
        std::printf("    %ux%u MiB:", w, h);
        for (int kind = 1; kind <= static_cast<int>(oma::compositor::FilterKind::Vignette);
             ++kind) {
            Layer l;
            l.looks = {{.kind = static_cast<oma::compositor::FilterKind>(kind), .amount = 1.0}};
            const double mib = added_mib(*device, a, w, h, l);
            std::printf(" look%d %+.1f", kind, mib);
            ok = ok && mib >= 0 && mib < 1.0;
        }
        for (const double detail : {-0.6, -1.0, 0.6}) {
            Layer l;
            l.sharpness = detail;
            const double mib = added_mib(*device, a, w, h, l);
            std::printf(" detail%+.1f %+.1f", detail, mib);
            ok = ok && mib >= 0 && mib < detail_bound;
        }
        std::printf("\n");
    }
    return ok;
}

void run_effect_memory_tests() {
    describe("Vulkan compositor memory per effect pass", {
        it("adds nothing for looks and at most two source-size images for detail",
           { expect(effects_stay_within_their_images()).toBeTruthy(); });
    });
}
