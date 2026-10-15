#include "nv12_pass.hpp"

#include "nv12_pass_detail.hpp"

#include "oma/gpu/device.hpp"

#include <array>

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
    const VkShaderModuleCreateInfo code{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                        .pNext = nullptr,
                                        .flags = 0,
                                        .codeSize = sizeof(kNv12Spv),
                                        .pCode = kNv12Spv.data()};
    VkShaderModule module = VK_NULL_HANDLE;
    if (const VkResult r = vkCreateShaderModule(dev, &code, nullptr, &module); r != VK_SUCCESS)
        return std::unexpected(nv12_failure(r, "cannot load the NV12 shader"));
    const VkComputePipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .pNext = nullptr,
                  .flags = 0,
                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = module,
                  .pName = "main",
                  .pSpecializationInfo = nullptr},
        .layout = p->layout_,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1};
    const VkResult made =
        vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &info, nullptr, &p->pipeline_);
    vkDestroyShaderModule(dev, module, nullptr);
    if (made != VK_SUCCESS)
        return std::unexpected(nv12_failure(made, "cannot create the NV12 pipeline"));
    return p;
}

} // namespace oma::compositor::detail
