#pragma once

#include "oma/base/error.hpp"
#include "oma/gpu/commands.hpp"
#include "oma/gpu/resources.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <span>

namespace oma::gpu {
class Device;
}

namespace oma::compositor::detail {

// The NV12 export pass (shaders/nv12_bt709.comp): its pipeline and the host-visible buffer the
// shader writes, so the bytes are read where they were written, with no image-to-buffer copy.
// Owned by one compositor and used on its thread; destroyed before the device.
class Nv12Pass {
public:
    struct Timings {
        gpu::CommandRunner::RunTimings commands;
        double invalidate_ms = 0;
        double gpu_convert_ms = 0; // timestamped dispatch, excludes time queued before it
        bool gpu_timestamp_available = false;
    };

    [[nodiscard]] static Result<std::unique_ptr<Nv12Pass>>
    create(const gpu::Device& device, PFN_vkCmdPushDescriptorSetKHR push_descriptors);
    ~Nv12Pass();
    Nv12Pass(const Nv12Pass&) = delete;
    Nv12Pass& operator=(const Nv12Pass&) = delete;
    Nv12Pass(Nv12Pass&&) = delete;
    Nv12Pass& operator=(Nv12Pass&&) = delete;

    // Converts `source` (width x height, even, VK_IMAGE_LAYOUT_GENERAL) and returns the
    // buffer's bytes: `stride` x height luma, then `stride` x height/2 CbCr. Valid until the
    // next run; the caller copies them out.
    [[nodiscard]] Result<std::span<const std::uint8_t>>
    run(gpu::CommandRunner& runner, VkImageView source, std::uint32_t width, std::uint32_t height,
        std::uint32_t stride, Timings* timings = nullptr);

private:
    Nv12Pass() = default;

    const gpu::Device* device_ = nullptr;
    PFN_vkCmdPushDescriptorSetKHR push_descriptors_ = nullptr;
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkQueryPool timestamps_ = VK_NULL_HANDLE;
    std::uint32_t timestamp_valid_bits_ = 0;
    double timestamp_period_ns_ = 0;
    gpu::Buffer buffer_;
};

} // namespace oma::compositor::detail
