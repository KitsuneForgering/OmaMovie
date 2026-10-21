// S7 (M12 step 3): can the GPU write straight into a VA-API *encode* surface? Creates an NV12
// surface for encoding, exports it as DMA-BUF (one layer per plane), imports each plane into
// OmaMovie's Vulkan device as a writable image with the surface's own DRM modifier, clears the
// planes to known values on the GPU, hands them back to VA-API and reads them through VA-API.
// Prints what the driver accepts at each step; exit 0 only if the read-back values match.
#include "oma/gpu/device.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdio>
#include <cstring>

namespace {

constexpr std::uint32_t kW = 1920;
constexpr std::uint32_t kH = 1080;
constexpr std::array<std::uint8_t, 3> kWant{128, 64, 191}; // Y, then U and V

bool fail(const char* what) {
    std::printf("FAIL: %s\n", what);
    return false;
}

// What the driver allows for `format` with `modifier` as a writable image.
bool supported(const oma::gpu::Device& d, VkFormat format, std::uint64_t modifier, VkImageUsageFlags usage) {
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
};

bool import_plane(const oma::gpu::Device& d, const VADRMPRIMESurfaceDescriptor& desc, std::uint32_t layer,
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
    return vkBindImageMemory(d.device(), out.image, out.memory, 0) == VK_SUCCESS || fail("vkBindImageMemory");
}

} // namespace

int main() {
    auto created = oma::gpu::Device::create();
    if (!created)
        return fail("no Vulkan device") ? 0 : 2;
    const oma::gpu::Device& d = **created;
    const int drm = open(d.info().render_node.c_str(), O_RDWR);
    VADisplay va = vaGetDisplayDRM(drm);
    int major = 0;
    int minor = 0;
    if (drm < 0 || vaInitialize(va, &major, &minor) != VA_STATUS_SUCCESS)
        return fail("vaInitialize") ? 0 : 2;
    std::printf("device: %s, VA-API %d.%d (%s)\n", d.info().name.c_str(), major, minor, vaQueryVendorString(va));

    std::array<VASurfaceAttrib, 2> attribs{};
    attribs[0] = {.type = VASurfaceAttribPixelFormat, .flags = VA_SURFACE_ATTRIB_SETTABLE,
                  .value = {.type = VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_NV12}}};
    attribs[1] = {.type = VASurfaceAttribUsageHint, .flags = VA_SURFACE_ATTRIB_SETTABLE,
                  .value = {.type = VAGenericValueTypeInteger,
                            .value = {.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER}}};
    VASurfaceID surface = VA_INVALID_SURFACE;
    if (vaCreateSurfaces(va, VA_RT_FORMAT_YUV420, kW, kH, &surface, 1, attribs.data(), 2) != VA_STATUS_SUCCESS)
        return fail("vaCreateSurfaces (NV12, encoder hint)") ? 0 : 1;
    VADRMPRIMESurfaceDescriptor desc{};
    if (vaExportSurfaceHandle(va, surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                              VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                              &desc) != VA_STATUS_SUCCESS)
        return fail("vaExportSurfaceHandle (read-write, separate layers)") ? 0 : 1;
    const std::uint64_t modifier = desc.objects[0].drm_format_modifier;
    std::printf("surface: %u objects, %u layers, modifier 0x%llx, Y pitch %u, UV offset %u\n", desc.num_objects,
                desc.num_layers, static_cast<unsigned long long>(modifier), desc.layers[0].pitch[0],
                desc.layers[1].offset[0]);
    for (const auto& [format, name] : {std::pair{VK_FORMAT_R8_UNORM, "R8"}, std::pair{VK_FORMAT_R8G8_UNORM, "RG8"}}) {
        std::printf("  %s with this modifier: transfer-dst %s, storage %s\n", name,
                    supported(d, format, modifier, VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? "yes" : "no",
                    supported(d, format, modifier, VK_IMAGE_USAGE_STORAGE_BIT) ? "yes" : "no");
    }
    std::array<Plane, 2> planes;
    if (!import_plane(d, desc, 0, VK_FORMAT_R8_UNORM, kW, kH, planes[0]) ||
        !import_plane(d, desc, 1, VK_FORMAT_R8G8_UNORM, kW / 2, kH / 2, planes[1]))
        return 1;

    // Clear Y and UV on the GPU: acquire from VA-API (foreign), clear, release back.
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
        const VkClearColorValue value{.float32 = {i == 0 ? kWant[0] / 255.0F : kWant[1] / 255.0F,
                                                  kWant[2] / 255.0F, 0, 0}};
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
    if (!submitted)
        return fail("GPU clear of the imported planes") ? 0 : 1;

    // Read back through VA-API, as the encoder would see the surface.
    vaSyncSurface(va, surface);
    VAImage image{};
    void* data = nullptr;
    if (vaDeriveImage(va, surface, &image) != VA_STATUS_SUCCESS ||
        vaMapBuffer(va, image.buf, &data) != VA_STATUS_SUCCESS)
        return fail("vaDeriveImage/vaMapBuffer") ? 0 : 1;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    const std::uint8_t y = bytes[image.offsets[0] + (image.pitches[0] * (kH / 2)) + (kW / 2)];
    const std::uint8_t* uv = bytes + image.offsets[1] + (image.pitches[1] * (kH / 4)) + (kW / 2);
    std::printf("read back through VA-API: Y %u, U %u, V %u (want %u %u %u)\n", y, uv[0], uv[1], kWant[0],
                kWant[1], kWant[2]);
    const bool ok = y == kWant[0] && uv[0] == kWant[1] && uv[1] == kWant[2];
    vaUnmapBuffer(va, image.buf);
    vaDestroyImage(va, image.image_id);
    for (Plane& p : planes) {
        vkDestroyImage(d.device(), p.image, nullptr);
        vkFreeMemory(d.device(), p.memory, nullptr);
    }
    vkDestroyCommandPool(d.device(), pool, nullptr);
    for (std::uint32_t i = 0; i < desc.num_objects; ++i)
        close(desc.objects[i].fd);
    vaDestroySurfaces(va, &surface, 1);
    vaTerminate(va);
    close(drm);
    std::printf("%s\n", ok ? "PASS: the GPU wrote the encode surface VA-API reads" : "FAIL: values differ");
    return ok ? 0 : 1;
}
