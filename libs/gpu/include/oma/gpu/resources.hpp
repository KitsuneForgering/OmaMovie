#pragma once

#include "oma/base/error.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>

// Owned Vulkan buffers and images (CLAUDE.md §7). Each holds a dedicated allocation for now;
// VMA replaces the allocation behind the same interface when allocation counts grow (M2 plan).
// Resources must be destroyed before the Device that created them.

namespace oma::gpu {

class Device;

enum class MemoryUse : std::uint8_t {
    GpuOnly,  // device-local
    Upload,   // host-visible, written by the CPU, read by the GPU
    Readback, // host-visible and cached, written by the GPU, read by the CPU
};

class Buffer {
public:
    Buffer() = default;
    [[nodiscard]] static Result<Buffer> create(const Device& device, VkDeviceSize size,
                                               VkBufferUsageFlags usage, MemoryUse memory);
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer();

    [[nodiscard]] VkBuffer handle() const noexcept { return buffer_; }
    [[nodiscard]] VkDeviceSize size() const noexcept { return size_; }
    // Persistently mapped pointer for Upload and Readback buffers, nullptr otherwise.
    [[nodiscard]] void* mapped() const noexcept { return mapped_; }
    // Makes CPU writes visible to the GPU / GPU writes visible to the CPU on non-coherent memory.
    void flush() const noexcept;
    void invalidate() const noexcept;

private:
    void reset() noexcept;

    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
    void* mapped_ = nullptr;
    bool coherent_ = true;
};

struct ImageDesc {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    std::uint32_t depth = 0; // above 0: a 3D image of `depth` slices (lookup tables)
};

// A 2D (or 3D, see ImageDesc::depth), single-mip, optimal-tiling, device-local image with a full
// color view.
class Image {
public:
    Image() = default;
    [[nodiscard]] static Result<Image> create(const Device& device, const ImageDesc& desc);
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    ~Image();

    [[nodiscard]] VkImage handle() const noexcept { return image_; }
    [[nodiscard]] VkImageView view() const noexcept { return view_; }
    [[nodiscard]] const ImageDesc& desc() const noexcept { return desc_; }

private:
    void reset() noexcept;

    VkDevice device_ = VK_NULL_HANDLE;
    VkImage image_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    ImageDesc desc_;
};

} // namespace oma::gpu
