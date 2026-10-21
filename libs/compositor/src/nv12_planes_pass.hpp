#pragma once

#include "oma/base/error.hpp"
#include "oma/gpu/commands.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>

namespace oma::gpu {
class Device;
}

namespace oma::compositor::detail {

// The GPU-native NV12 export pass (shaders/nv12_planes.comp, M12): the NV12 of nv12_bt709.comp
// written into two caller-owned storage images, R8 luma and RG8 CbCr, instead of a host buffer.
// Owned by one compositor and used on its thread; destroyed before the device.
class Nv12PlanesPass {
public:
    struct Planes {
        VkImage luma = VK_NULL_HANDLE;
        VkImageView luma_view = VK_NULL_HANDLE;
        VkImage cbcr = VK_NULL_HANDLE;
        VkImageView cbcr_view = VK_NULL_HANDLE;
        bool foreign = false; // acquired from and released to VK_QUEUE_FAMILY_FOREIGN_EXT
    };

    [[nodiscard]] static Result<std::unique_ptr<Nv12PlanesPass>>
    create(const gpu::Device& device, PFN_vkCmdPushDescriptorSetKHR push_descriptors);
    ~Nv12PlanesPass();
    Nv12PlanesPass(const Nv12PlanesPass&) = delete;
    Nv12PlanesPass& operator=(const Nv12PlanesPass&) = delete;
    Nv12PlanesPass(Nv12PlanesPass&&) = delete;
    Nv12PlanesPass& operator=(Nv12PlanesPass&&) = delete;

    // Converts `source` (width x height, even, VK_IMAGE_LAYOUT_GENERAL) into `planes`, whose old
    // contents are discarded; returns when the GPU has finished, with both in GENERAL.
    [[nodiscard]] Result<void> run(gpu::CommandRunner& runner, VkImageView source,
                                   std::uint32_t width, std::uint32_t height, const Planes& planes);

private:
    Nv12PlanesPass() = default;

    const gpu::Device* device_ = nullptr;
    PFN_vkCmdPushDescriptorSetKHR push_descriptors_ = nullptr;
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace oma::compositor::detail
