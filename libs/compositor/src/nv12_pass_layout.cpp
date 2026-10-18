#include "nv12_pass_detail.hpp"

#include <array>

namespace oma::compositor::detail {

Result<void> create_nv12_layouts(VkDevice device, VkDescriptorSetLayout& set,
                                 VkPipelineLayout& layout) {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    for (std::uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i] = {.binding = i,
                       .descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                                : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                       .descriptorCount = 1,
                       .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                       .pImmutableSamplers = nullptr};
    }
    const VkDescriptorSetLayoutCreateInfo set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()};
    if (const VkResult r = vkCreateDescriptorSetLayout(device, &set_info, nullptr, &set);
        r != VK_SUCCESS)
        return std::unexpected(nv12_failure(r, "cannot create the NV12 descriptor layout"));
    const VkPushConstantRange stride{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(std::uint32_t)};
    const VkPipelineLayoutCreateInfo layout_info{.sType =
                                                     VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .setLayoutCount = 1,
                                                 .pSetLayouts = &set,
                                                 .pushConstantRangeCount = 1,
                                                 .pPushConstantRanges = &stride};
    if (const VkResult r = vkCreatePipelineLayout(device, &layout_info, nullptr, &layout);
        r != VK_SUCCESS)
        return std::unexpected(nv12_failure(r, "cannot create the NV12 pipeline layout"));
    return {};
}

} // namespace oma::compositor::detail
