#include "nv12_planes_pass.hpp"

#include "nv12_pass_detail.hpp"

#include "oma/gpu/device.hpp"

#include <array>

namespace oma::compositor::detail {

namespace {
constexpr auto kPlanesSpv = std::to_array<std::uint32_t>({
#include "nv12_planes.comp.inc"
});
constexpr std::uint32_t kGroup = 16; // nv12_planes.comp's local size, in 2x2 pixel blocks

// Both planes from `from` to `to` around the write: the layout is GENERAL after it; with a
// foreign owner the transfer also moves the images between that owner and this queue family.
void transition(VkCommandBuffer cmd, const Nv12PlanesPass::Planes& planes, std::uint32_t family,
                bool acquire) {
    const std::uint32_t other =
        planes.foreign ? VK_QUEUE_FAMILY_FOREIGN_EXT : VK_QUEUE_FAMILY_IGNORED;
    const std::uint32_t here = planes.foreign ? family : VK_QUEUE_FAMILY_IGNORED;
    std::array<VkImageMemoryBarrier2, 2> barriers{};
    for (std::size_t i = 0; i < barriers.size(); ++i) {
        barriers[i] = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask =
                acquire ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = acquire ? VK_ACCESS_2_NONE : VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask =
                acquire ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_2_NONE,
            .dstAccessMask = acquire ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT : VK_ACCESS_2_NONE,
            .oldLayout = acquire ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = acquire ? other : here,
            .dstQueueFamilyIndex = acquire ? here : other,
            .image = i == 0 ? planes.luma : planes.cbcr,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}};
    }
    const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                               .pNext = nullptr,
                               .dependencyFlags = 0,
                               .memoryBarrierCount = 0,
                               .pMemoryBarriers = nullptr,
                               .bufferMemoryBarrierCount = 0,
                               .pBufferMemoryBarriers = nullptr,
                               .imageMemoryBarrierCount =
                                   static_cast<std::uint32_t>(barriers.size()),
                               .pImageMemoryBarriers = barriers.data()};
    vkCmdPipelineBarrier2(cmd, &dep);
}
} // namespace

Nv12PlanesPass::~Nv12PlanesPass() {
    VkDevice dev = device_->device();
    vkDestroyPipeline(dev, pipeline_, nullptr);
    vkDestroyPipelineLayout(dev, layout_, nullptr);
    vkDestroyDescriptorSetLayout(dev, set_layout_, nullptr);
}

Result<std::unique_ptr<Nv12PlanesPass>>
Nv12PlanesPass::create(const gpu::Device& device, PFN_vkCmdPushDescriptorSetKHR push_descriptors) {
    std::unique_ptr<Nv12PlanesPass> p(new Nv12PlanesPass());
    p->device_ = &device;
    p->push_descriptors_ = push_descriptors;
    VkDevice dev = device.device();
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    for (std::uint32_t i = 0; i < bindings.size(); ++i)
        bindings[i] = {.binding = i,
                       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                       .descriptorCount = 1,
                       .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                       .pImmutableSamplers = nullptr};
    const VkDescriptorSetLayoutCreateInfo set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    if (const VkResult r = vkCreateDescriptorSetLayout(dev, &set_info, nullptr, &p->set_layout_);
        r != VK_SUCCESS)
        return std::unexpected(nv12_failure(r, "cannot create the NV12 planes descriptor layout"));
    const VkPipelineLayoutCreateInfo layout_info{.sType =
                                                     VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .setLayoutCount = 1,
                                                 .pSetLayouts = &p->set_layout_,
                                                 .pushConstantRangeCount = 0,
                                                 .pPushConstantRanges = nullptr};
    if (const VkResult r = vkCreatePipelineLayout(dev, &layout_info, nullptr, &p->layout_);
        r != VK_SUCCESS)
        return std::unexpected(nv12_failure(r, "cannot create the NV12 planes pipeline layout"));
    auto pipeline = nv12_pipeline(dev, kPlanesSpv, p->layout_);
    if (!pipeline)
        return std::unexpected(pipeline.error());
    p->pipeline_ = *pipeline;
    return p;
}

Result<void> Nv12PlanesPass::run(gpu::CommandRunner& runner, VkImageView source,
                                 std::uint32_t width, std::uint32_t height, const Planes& planes) {
    const std::uint32_t family = device_->graphics_family();
    return runner.run([&](VkCommandBuffer cmd) {
        transition(cmd, planes, family, true);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        const std::array<VkDescriptorImageInfo, 3> images{
            VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                  .imageView = source,
                                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL},
            VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                  .imageView = planes.luma_view,
                                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL},
            VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                  .imageView = planes.cbcr_view,
                                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL}};
        std::array<VkWriteDescriptorSet, 3> writes{};
        for (std::uint32_t b = 0; b < writes.size(); ++b)
            writes[b] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                         .pNext = nullptr,
                         .dstSet = VK_NULL_HANDLE,
                         .dstBinding = b,
                         .dstArrayElement = 0,
                         .descriptorCount = 1,
                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         .pImageInfo = &images[b],
                         .pBufferInfo = nullptr,
                         .pTexelBufferView = nullptr};
        push_descriptors_(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout_, 0,
                          static_cast<std::uint32_t>(writes.size()), writes.data());
        vkCmdDispatch(cmd, ((width / 2) + kGroup - 1) / kGroup,
                      ((height / 2) + kGroup - 1) / kGroup, 1);
        transition(cmd, planes, family, false);
    });
}

} // namespace oma::compositor::detail
