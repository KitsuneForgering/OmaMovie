#include "nv12_pass.hpp"

#include "nv12_pass_detail.hpp"

#include "oma/gpu/device.hpp"

#include <array>
#include <vector>

namespace oma::compositor::detail {

namespace {
constexpr auto kNv12Spv = std::to_array<std::uint32_t>({
#include "nv12_bt709.comp.inc"
});
} // namespace

Result<std::unique_ptr<Nv12Pass>> Nv12Pass::create(const gpu::Device& device,
                                                   PFN_vkCmdPushDescriptorSetKHR push_descriptors) {
    std::unique_ptr<Nv12Pass> p(new Nv12Pass());
    p->device_ = &device;
    p->push_descriptors_ = push_descriptors;
    VkDevice dev = device.device();
    if (auto r = create_nv12_layouts(dev, p->set_layout_, p->layout_); !r)
        return std::unexpected(r.error());
    auto pipeline = nv12_pipeline(dev, kNv12Spv, p->layout_);
    if (!pipeline)
        return std::unexpected(pipeline.error());
    p->pipeline_ = *pipeline;

    // Optional diagnostic: keep export functional when a queue cannot provide timestamps.
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device(), &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device(), &family_count,
                                             families.data());
    if (device.graphics_family() < family_count) {
        p->timestamp_valid_bits_ = families[device.graphics_family()].timestampValidBits;
        if (p->timestamp_valid_bits_ != 0) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device.physical_device(), &properties);
            p->timestamp_period_ns_ = static_cast<double>(properties.limits.timestampPeriod);
            const VkQueryPoolCreateInfo query{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                              .queryCount = 2,
                                              .pipelineStatistics = 0};
            if (vkCreateQueryPool(dev, &query, nullptr, &p->timestamps_) != VK_SUCCESS)
                p->timestamps_ = VK_NULL_HANDLE;
        }
    }
    return p;
}

} // namespace oma::compositor::detail
