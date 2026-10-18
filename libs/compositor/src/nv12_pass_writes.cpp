#include "nv12_pass_detail.hpp"

#include "oma/gpu/commands.hpp"

namespace oma::compositor::detail {

Error nv12_failure(VkResult r, std::string what) {
    return {gpu::error_code(r), Category::Compositor, std::move(what),
            "VkResult " + std::to_string(r)};
}

std::array<VkWriteDescriptorSet, 2> nv12_writes(const VkDescriptorImageInfo& image,
                                                const VkDescriptorBufferInfo& range) {
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t b = 0; b < 2; ++b) {
        writes[b] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                     .pNext = nullptr,
                     .dstSet = VK_NULL_HANDLE,
                     .dstBinding = b,
                     .dstArrayElement = 0,
                     .descriptorCount = 1,
                     .descriptorType = b == 0 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                              : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     .pImageInfo = b == 0 ? &image : nullptr,
                     .pBufferInfo = b == 0 ? nullptr : &range,
                     .pTexelBufferView = nullptr};
    }
    return writes;
}

void nv12_host_barrier(VkCommandBuffer cmd) {
    const VkMemoryBarrier2 done{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                .pNext = nullptr,
                                .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                                .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                                .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT};
    const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                               .pNext = nullptr,
                               .dependencyFlags = 0,
                               .memoryBarrierCount = 1,
                               .pMemoryBarriers = &done,
                               .bufferMemoryBarrierCount = 0,
                               .pBufferMemoryBarriers = nullptr,
                               .imageMemoryBarrierCount = 0,
                               .pImageMemoryBarriers = nullptr};
    vkCmdPipelineBarrier2(cmd, &dep);
}

} // namespace oma::compositor::detail
