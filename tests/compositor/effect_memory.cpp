#include "effect_memory.hpp"

#include <array>

using oma::compositor::Fit;
using oma::compositor::Layer;
using oma::compositor::RenderGraph;
using oma::compositor::VulkanCompositor;

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
