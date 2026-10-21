#pragma once

// Shared by the M12 VA-API spikes (s7, s8): an NV12 VA-API surface's planes imported into
// OmaMovie's Vulkan device as writable images, and a GPU clear of them between acquiring them
// from VA-API (the foreign queue family) and releasing them back.
#include "oma/gpu/device.hpp"

#include <unistd.h>
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <cstdio>

namespace va_planes {

inline bool fail(const char* what) {
    std::printf("FAIL: %s\n", what);
    return false;
}

// What the driver allows for `format` with `modifier` as a writable image.
inline bool supported(const oma::gpu::Device& d, VkFormat format, std::uint64_t modifier, VkImageUsageFlags usage) {
    const VkPhysicalDeviceImageDrmFormatModifierInfoEXT mod{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
        .pNext = nullptr, .drmFormatModifier = modifier, .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0, .pQueueFamilyIndices = nullptr};
    const VkPhysicalDeviceExternalImageFormatInfo ext{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO, .pNext = &mod,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkPhysicalDeviceImageFormatInfo2 info{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, .pNext = &ext, .format = format,
        .type = VK_IMAGE_TYPE_2D, .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, .usage = usage, .flags = 0};
    VkImageFormatProperties2 props{.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, .pNext = nullptr,
                                   .imageFormatProperties = {}};
    return vkGetPhysicalDeviceImageFormatProperties2(d.physical_device(), &info, &props) == VK_SUCCESS;
}

struct Plane {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE; // for storage writes (VulkanCompositor::write_nv12)
};

inline void destroy(const oma::gpu::Device& d, Plane& p) {
    vkDestroyImageView(d.device(), p.view, nullptr);
    vkDestroyImage(d.device(), p.image, nullptr);
    vkFreeMemory(d.device(), p.memory, nullptr);
    p = {};
}

inline bool import_plane(const oma::gpu::Device& d, const VADRMPRIMESurfaceDescriptor& desc, std::uint32_t layer,
                  VkFormat format, std::uint32_t w, std::uint32_t h, Plane& out) {
    const auto& l = desc.layers[layer];
    const auto& obj = desc.objects[l.object_index[0]];
    const VkSubresourceLayout plane{.offset = l.offset[0], .size = 0, .rowPitch = l.pitch[0], .arrayPitch = 0,
                                    .depthPitch = 0};
    const VkImageDrmFormatModifierExplicitCreateInfoEXT mod{
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT, .pNext = nullptr,
        .drmFormatModifier = obj.drm_format_modifier, .drmFormatModifierPlaneCount = 1, .pPlaneLayouts = &plane};
    const VkExternalMemoryImageCreateInfo ext{.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                                              .pNext = &mod,
                                              .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkImageCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = &ext, .flags = 0,
                                 .imageType = VK_IMAGE_TYPE_2D, .format = format, .extent = {w, h, 1},
                                 .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                                 .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                 .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .queueFamilyIndexCount = 0,
                                 .pQueueFamilyIndices = nullptr, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    if (vkCreateImage(d.device(), &info, nullptr, &out.image) != VK_SUCCESS)
        return fail("vkCreateImage over the VA plane");
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(d.device(), out.image, &req);
    const VkImportMemoryFdInfoKHR fd{.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, .pNext = nullptr,
                                     .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                     .fd = dup(obj.fd)};
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(d.physical_device(), &mem);
    std::uint32_t type = 0;
    while (type < mem.memoryTypeCount && (req.memoryTypeBits & (1U << type)) == 0)
        ++type;
    const VkMemoryAllocateInfo alloc{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &fd,
                                     .allocationSize = req.size, .memoryTypeIndex = type};
    if (vkAllocateMemory(d.device(), &alloc, nullptr, &out.memory) != VK_SUCCESS)
        return fail("importing the plane's DMA-BUF");
    if (vkBindImageMemory(d.device(), out.image, out.memory, 0) != VK_SUCCESS)
        return fail("vkBindImageMemory");
    const VkImageViewCreateInfo view{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr,
                                     .flags = 0, .image = out.image, .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                     .format = format, .components = {},
                                     .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    return vkCreateImageView(d.device(), &view, nullptr, &out.view) == VK_SUCCESS || fail("vkCreateImageView");
}


// Clears Y to `y` and the interleaved chroma to (u, v), 8-bit values, and waits for the GPU.
inline bool clear_planes(const oma::gpu::Device& d, const std::array<Plane, 2>& planes, std::uint8_t y,
                         std::uint8_t u, std::uint8_t v) {
    const std::uint32_t family = d.graphics_family();
    const VkCommandPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = nullptr,
                                            .flags = 0, .queueFamilyIndex = family};
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(d.device(), &pool_info, nullptr, &pool);
    const VkCommandBufferAllocateInfo cb_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .pNext = nullptr, .commandPool = pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(d.device(), &cb_info, &cb);
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = nullptr,
                                         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                         .pInheritanceInfo = nullptr};
    vkBeginCommandBuffer(cb, &begin);
    const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    for (std::size_t i = 0; i < planes.size(); ++i) {
        VkImageMemoryBarrier acquire{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = nullptr,
                                     .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT, .dstQueueFamilyIndex = family,
                                     .image = planes[i].image, .subresourceRange = all};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &acquire);
        const VkClearColorValue value{
            .float32 = {static_cast<float>(i == 0 ? y : u) / 255.0F, static_cast<float>(v) / 255.0F, 0, 0}};
        vkCmdClearColorImage(cb, planes[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &all);
        VkImageMemoryBarrier release = acquire;
        release.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        release.dstAccessMask = 0;
        release.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        release.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        release.srcQueueFamilyIndex = family;
        release.dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &release);
    }
    vkEndCommandBuffer(cb);
    const VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = nullptr, .waitSemaphoreCount = 0,
                              .pWaitSemaphores = nullptr, .pWaitDstStageMask = nullptr, .commandBufferCount = 1,
                              .pCommandBuffers = &cb, .signalSemaphoreCount = 0, .pSignalSemaphores = nullptr};
    d.lock_queue(family, 0);
    const bool submitted = vkQueueSubmit(d.queue(family, 0), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                           vkQueueWaitIdle(d.queue(family, 0)) == VK_SUCCESS;
    d.unlock_queue(family, 0);
    vkDestroyCommandPool(d.device(), pool, nullptr);
    return submitted || fail("GPU clear of the imported planes");

}

} // namespace va_planes
