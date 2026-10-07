#include "oma/gpu/commands.hpp"

#include "oma/gpu/device.hpp"

#include <string>
#include <utility>
#include <vector>

namespace oma::gpu {

namespace {

Error vk_failure(VkResult r, std::string what) {
    return {r == VK_ERROR_DEVICE_LOST ? ErrorCode::DeviceLost : ErrorCode::Internal, Category::Gpu,
            std::move(what), "VkResult " + std::to_string(static_cast<int>(r))};
}

// A failed submission or wait; a lost device is marked so producers stop (Device::lost).
Error submit_failure(const Device& device, VkResult r, std::string what) {
    if (r == VK_ERROR_DEVICE_LOST) {
        device.mark_lost();
    }
    return vk_failure(r, std::move(what));
}

Error lost_device() {
    return {ErrorCode::DeviceLost, Category::Gpu, "the GPU device was lost"};
}

std::vector<VkSemaphoreSubmitInfo> to_infos(std::span<const SemaphoreSubmit> in) {
    std::vector<VkSemaphoreSubmitInfo> out;
    out.reserve(in.size());
    for (const SemaphoreSubmit& s : in) {
        out.push_back({.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                       .pNext = nullptr,
                       .semaphore = s.semaphore,
                       .value = s.value,
                       .stageMask = s.stage,
                       .deviceIndex = 0});
    }
    return out;
}

} // namespace

Result<CommandRunner> CommandRunner::create(const Device& device, std::uint32_t family) {
    CommandRunner r;
    r.device_ = &device;
    r.family_ = family;
    r.queue_ = device.queue(family, 0);
    const VkCommandPoolCreateInfo pool{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                       .pNext = nullptr,
                                       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                       .queueFamilyIndex = family};
    if (const VkResult res = vkCreateCommandPool(device.device(), &pool, nullptr, &r.pool_);
        res != VK_SUCCESS) {
        return std::unexpected(vk_failure(res, "cannot create a command pool"));
    }
    const VkCommandBufferAllocateInfo alloc{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                            .pNext = nullptr,
                                            .commandPool = r.pool_,
                                            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                            .commandBufferCount = 1};
    if (const VkResult res = vkAllocateCommandBuffers(device.device(), &alloc, &r.cmd_);
        res != VK_SUCCESS) {
        return std::unexpected(vk_failure(res, "cannot allocate a command buffer"));
    }
    const VkFenceCreateInfo fence{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
    if (const VkResult res = vkCreateFence(device.device(), &fence, nullptr, &r.fence_);
        res != VK_SUCCESS) {
        return std::unexpected(vk_failure(res, "cannot create a fence"));
    }
    return r;
}

CommandRunner::CommandRunner(CommandRunner&& other) noexcept
    : device_(std::exchange(other.device_, nullptr)), family_(other.family_),
      queue_(std::exchange(other.queue_, VK_NULL_HANDLE)),
      pool_(std::exchange(other.pool_, VK_NULL_HANDLE)),
      cmd_(std::exchange(other.cmd_, VK_NULL_HANDLE)),
      fence_(std::exchange(other.fence_, VK_NULL_HANDLE)) {}

CommandRunner& CommandRunner::operator=(CommandRunner&& other) noexcept {
    if (this != &other) {
        reset();
        device_ = std::exchange(other.device_, nullptr);
        family_ = other.family_;
        queue_ = std::exchange(other.queue_, VK_NULL_HANDLE);
        pool_ = std::exchange(other.pool_, VK_NULL_HANDLE);
        cmd_ = std::exchange(other.cmd_, VK_NULL_HANDLE);
        fence_ = std::exchange(other.fence_, VK_NULL_HANDLE);
    }
    return *this;
}

CommandRunner::~CommandRunner() {
    reset();
}

void CommandRunner::reset() noexcept {
    if (device_ == nullptr) {
        return;
    }
    vkDestroyFence(device_->device(), fence_, nullptr);
    vkDestroyCommandPool(device_->device(), pool_, nullptr); // frees the command buffer
    device_ = nullptr;
}

Result<void> CommandRunner::run(const std::function<void(VkCommandBuffer)>& record,
                                std::span<const SemaphoreSubmit> waits,
                                std::span<const SemaphoreSubmit> signals) {
    if (device_->lost()) {
        return std::unexpected(lost_device()); // a lost device accepts no more work
    }
    vkResetCommandBuffer(cmd_, 0);
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                         .pNext = nullptr,
                                         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                         .pInheritanceInfo = nullptr};
    if (const VkResult r = vkBeginCommandBuffer(cmd_, &begin); r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot begin a command buffer"));
    }
    record(cmd_);
    if (const VkResult r = vkEndCommandBuffer(cmd_); r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot record commands"));
    }
    const auto wait_infos = to_infos(waits);
    const auto signal_infos = to_infos(signals);
    const VkCommandBufferSubmitInfo cbi{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                        .pNext = nullptr,
                                        .commandBuffer = cmd_,
                                        .deviceMask = 0};
    const VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = static_cast<std::uint32_t>(wait_infos.size()),
        .pWaitSemaphoreInfos = wait_infos.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cbi,
        .signalSemaphoreInfoCount = static_cast<std::uint32_t>(signal_infos.size()),
        .pSignalSemaphoreInfos = signal_infos.data()};
    device_->lock_queue(family_, 0);
    const VkResult submitted = vkQueueSubmit2(queue_, 1, &submit, fence_);
    device_->unlock_queue(family_, 0);
    if (submitted != VK_SUCCESS) {
        return std::unexpected(submit_failure(*device_, submitted, "queue submission failed"));
    }
    const VkResult waited = vkWaitForFences(device_->device(), 1, &fence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_->device(), 1, &fence_);
    if (waited != VK_SUCCESS) {
        return std::unexpected(submit_failure(*device_, waited, "waiting for the GPU failed"));
    }
    return {};
}

namespace {

void foreign_barrier(VkCommandBuffer cmd, VkImage image, std::uint32_t from, std::uint32_t to,
                     bool acquire) {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = acquire ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = acquire ? VK_ACCESS_2_NONE : VK_ACCESS_2_MEMORY_READ_BIT,
        .dstStageMask = acquire ? VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = acquire ? VK_ACCESS_2_MEMORY_READ_BIT : VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = from,
        .dstQueueFamilyIndex = to,
        .image = image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                               .pNext = nullptr,
                               .dependencyFlags = 0,
                               .memoryBarrierCount = 0,
                               .pMemoryBarriers = nullptr,
                               .bufferMemoryBarrierCount = 0,
                               .pBufferMemoryBarriers = nullptr,
                               .imageMemoryBarrierCount = 1,
                               .pImageMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(cmd, &dep);
}

} // namespace

void acquire_foreign(VkCommandBuffer cmd, VkImage image, std::uint32_t family) {
    foreign_barrier(cmd, image, VK_QUEUE_FAMILY_FOREIGN_EXT, family, true);
}

void release_foreign(VkCommandBuffer cmd, VkImage image, std::uint32_t family) {
    foreign_barrier(cmd, image, family, VK_QUEUE_FAMILY_FOREIGN_EXT, false);
}

void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                VkImageAspectFlags aspect) {
    const VkImageMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                        .pNext = nullptr,
                                        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                                        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                        .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
                                                         VK_ACCESS_2_MEMORY_WRITE_BIT,
                                        .oldLayout = from,
                                        .newLayout = to,
                                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        .image = image,
                                        .subresourceRange = {.aspectMask = aspect,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
    const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                               .pNext = nullptr,
                               .dependencyFlags = 0,
                               .memoryBarrierCount = 0,
                               .pMemoryBarriers = nullptr,
                               .bufferMemoryBarrierCount = 0,
                               .pBufferMemoryBarriers = nullptr,
                               .imageMemoryBarrierCount = 1,
                               .pImageMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(cmd, &dep);
}

} // namespace oma::gpu
