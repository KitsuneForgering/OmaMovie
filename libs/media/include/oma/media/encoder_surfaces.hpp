#pragma once

// GPU-native input for the VA-API encoder (ADR-0023): surfaces of the encoder's own pool whose
// NV12 planes are writable images on a Vulkan device, filled there (for example by
// compositor::VulkanCompositor::write_nv12, which must treat them as foreign-owned) and then
// encoded with VideoWriter::write_surface. No host copy and no upload.

#include "oma/base/error.hpp"

#include <vulkan/vulkan_core.h>

#include <memory>

namespace oma::gpu {
class Device;
}

namespace oma::media {

// One free encoder surface: its planes on the device, and the surface itself for the writer.
struct EncoderSurface {
    VkImage luma = VK_NULL_HANDLE; // R8, the frame's size
    VkImageView luma_view = VK_NULL_HANDLE;
    VkImage cbcr = VK_NULL_HANDLE; // RG8 interleaved CbCr, half size
    VkImageView cbcr_view = VK_NULL_HANDLE;
    std::shared_ptr<void> frame; // the encoder's picture (opaque); VideoWriter::write_surface
};

// Made by VideoWriter::surfaces. Each pool surface's planes are imported once and kept until
// this is destroyed, which must happen before the device. Used by one thread at a time; the
// writer may encode on another (write_surface) meanwhile.
class EncoderSurfaces {
public:
    ~EncoderSurfaces();
    EncoderSurfaces(const EncoderSurfaces&) = delete;
    EncoderSurfaces& operator=(const EncoderSurfaces&) = delete;
    EncoderSurfaces(EncoderSurfaces&&) = delete;
    EncoderSurfaces& operator=(EncoderSurfaces&&) = delete;

    // A surface the encoder no longer reads, its planes ready to be written (their old contents
    // discarded). It returns to the pool once encoded, or when the handle is dropped unencoded.
    [[nodiscard]] Result<EncoderSurface> acquire();

    struct Impl;
    explicit EncoderSurfaces(std::unique_ptr<Impl> impl);

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::media
