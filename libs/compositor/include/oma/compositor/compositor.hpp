#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/compositor/render_graph.hpp"
#include "oma/media/probe.hpp"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace oma::gpu {
class Device;
class Image;
} // namespace oma::gpu

namespace oma::media {
class VideoFrame;
}

namespace oma::compositor {

// One source of the frame being composed: a decoded picture plus how to interpret it.
struct LayerInput {
    media::VideoFrame* frame = nullptr; // GPU or software frame; must outlive the render call
    media::ColorInfo color;
    int rotation = 0; // counterclockwise display rotation from the stream
    Rational sample_aspect = Rational::literal(1, 1);
};

// Linear-light BT.709 RGBA with premultiplied alpha, one float per channel, rows top first.
struct RgbaImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> pixels;

    [[nodiscard]] std::array<float, 4> at(std::uint32_t x, std::uint32_t y) const noexcept;
};

// The correctness reference (CLAUDE.md §9.3): the same per-pixel math as the GPU compositor,
// on software frames, in plain C++. Slow; for tests and GPU-less diagnostics.
class CpuCompositor {
public:
    [[nodiscard]] Result<RgbaImage> render(const RenderGraph& graph,
                                           std::span<const LayerInput> inputs) const;
};

// Executes render graphs on OmaMovie's device into an RGBA16F output image (linear,
// premultiplied), taking GPU frames without copies and uploading software frames.
// Synchronous for now: render() returns when the GPU has finished. Not thread-safe.
class VulkanCompositor {
public:
    [[nodiscard]] static Result<std::unique_ptr<VulkanCompositor>>
    create(const gpu::Device& device);

    ~VulkanCompositor();
    VulkanCompositor(const VulkanCompositor&) = delete;
    VulkanCompositor& operator=(const VulkanCompositor&) = delete;
    VulkanCompositor(VulkanCompositor&&) = delete;
    VulkanCompositor& operator=(VulkanCompositor&&) = delete;

    struct RenderTransferStats {
        std::uint64_t staged_inputs = 0; // unique software inputs copied for this render
        std::uint64_t staged_bytes = 0;  // packed rows copied to upload memory
        double staging_ms = 0;           // allocation, CPU copy and flush
        double row_copy_ms = 0;
    };
    [[nodiscard]] Result<void> render(const RenderGraph& graph, std::span<const LayerInput> inputs,
                                      RenderTransferStats* transfers = nullptr);

    // The last render's result, in VK_IMAGE_LAYOUT_GENERAL. Valid until the next render.
    [[nodiscard]] const gpu::Image* output() const noexcept;

    // Dedicated Vulkan allocation sizes of the live blur/sharpen intermediates. This excludes
    // driver caches and other users of the device, unlike VK_EXT_memory_budget heap usage.
    [[nodiscard]] std::uint64_t detail_image_bytes() const noexcept;

    // Copies the output back to memory (tests, thumbnails). Slow by design.
    [[nodiscard]] Result<RgbaImage> read_output();

    // Preview display transform: encodes the last render with the sRGB transfer function into
    // an RGBA8 image (VK_IMAGE_LAYOUT_GENERAL, sampleable, same size as the output) for an SDR
    // display. Returns when the GPU has finished; the image stays valid until the next call.
    // Later work on this queue (for example Qt sampling it) is ordered after the encode, and the
    // next encode waits for earlier work on the queue before overwriting it. The full SDR/HDR
    // display policy is ADR-0006's. `Bt709` encodes for BT.709 video instead (export): the
    // inverse of the BT.1886 EOTF that decoding BT.709 video applies, so levels round-trip.
    enum class Transfer : std::uint8_t { Srgb, Bt709 };
    [[nodiscard]] Result<const gpu::Image*> encode_display(Transfer transfer = Transfer::Srgb);

    // RGBA8 pixels of the last encode_display() (tests, export's software encoder).
    [[nodiscard]] Result<std::vector<std::uint8_t>> read_display();

    // The last render as NV12 for a hardware encoder (export): BT.709 matrix, limited range,
    // the Bt709 transfer above, chroma the mean of each 2x2 block. `stride`-byte luma rows,
    // then `stride`-byte interleaved CbCr rows at half height. Even sizes only (Unsupported).
    struct Nv12Image {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t stride = 0; // a multiple of 4
        std::vector<std::uint8_t> bytes;
    };
    struct Nv12ReadbackTimings {
        double record_ms = 0;
        double submit_ms = 0;
        double fence_wait_ms = 0;  // includes queued GPU work, not isolated shader time
        double gpu_convert_ms = 0; // Vulkan timestamps around NV12 dispatch, if available
        bool gpu_timestamp_available = false;
        double invalidate_ms = 0;
        double host_copy_ms = 0;
    };
    [[nodiscard]] Result<Nv12Image> read_nv12(Nv12ReadbackTimings* timings = nullptr);

    // The last render as the same NV12, written on the GPU into two caller-owned storage images
    // instead of read back (M12): `luma` R8 width x height, `cbcr` RG8 half size, such as the
    // planes of a hardware encoder's surface imported into this device. Their old contents are
    // discarded; both are left in VK_IMAGE_LAYOUT_GENERAL. With `foreign` another driver owns
    // them: they are acquired from and released back to VK_QUEUE_FAMILY_FOREIGN_EXT around the
    // write. Returns when the GPU has finished. Even sizes only (Unsupported).
    struct Nv12Planes {
        VkImage luma = VK_NULL_HANDLE;
        VkImageView luma_view = VK_NULL_HANDLE;
        VkImage cbcr = VK_NULL_HANDLE;
        VkImageView cbcr_view = VK_NULL_HANDLE;
        bool foreign = false;
    };
    [[nodiscard]] Result<void> write_nv12(const Nv12Planes& planes);

private:
    struct Impl;
    explicit VulkanCompositor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::compositor
