#pragma once

#include "oma/base/error.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <functional>
#include <span>

namespace oma::gpu {

class Device;

struct SemaphoreSubmit {
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::uint64_t value = 0; // timeline value; ignored by binary semaphores
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
};

// One reusable command buffer on queue 0 of a family. run() records, submits under the device's
// queue lock and waits for completion: simple and synchronous, for setup, uploads, tests and the
// first compositor. Not thread-safe; one runner per thread.
class CommandRunner {
public:
    [[nodiscard]] static Result<CommandRunner> create(const Device& device, std::uint32_t family);

    CommandRunner(CommandRunner&& other) noexcept;
    CommandRunner& operator=(CommandRunner&& other) noexcept;
    CommandRunner(const CommandRunner&) = delete;
    CommandRunner& operator=(const CommandRunner&) = delete;
    ~CommandRunner();

    [[nodiscard]] Result<void> run(const std::function<void(VkCommandBuffer)>& record,
                                   std::span<const SemaphoreSubmit> waits = {},
                                   std::span<const SemaphoreSubmit> signals = {});

private:
    CommandRunner() = default;
    void reset() noexcept;

    const Device* device_ = nullptr;
    std::uint32_t family_ = 0;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool pool_ = VK_NULL_HANDLE;
    VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
};

// Ownership of another driver's image (an imported DMA-BUF) for one submission: acquire from
// VK_QUEUE_FAMILY_FOREIGN_EXT before reading it on `family`, release back after. The layout stays
// GENERAL, the layout external memory is shared in.
void acquire_foreign(VkCommandBuffer cmd, VkImage image, std::uint32_t family);
void release_foreign(VkCommandBuffer cmd, VkImage image, std::uint32_t family);

// Records a full-subresource layout transition of a color image (synchronization2).
void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

} // namespace oma::gpu
