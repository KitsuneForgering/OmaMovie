#include "effect_memory.hpp"

#include <array>

using oma::compositor::Fit;
using oma::compositor::Layer;
using oma::compositor::RenderGraph;
using oma::compositor::VulkanCompositor;

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
    if (!(*vk)->render(g, inputs))
        return -1;
    const double before = static_cast<double>((*vk)->detail_image_bytes());
    g.layers[0] = layer;
    g.layers[0].fit = Fit::Fill;
    if (!(*vk)->render(g, inputs))
        return -1;
    return (static_cast<double>((*vk)->detail_image_bytes()) - before) / (1 << 20);
}
