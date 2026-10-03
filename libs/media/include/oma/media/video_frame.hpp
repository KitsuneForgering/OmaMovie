#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace oma::media {

// How a frame was decoded (Docs/spikes/S1-*.md, S2-*.md). Hardware paths deliver Vulkan images
// on OmaMovie's device without a CPU copy; software is the correctness reference and fallback.
enum class DecodePath : std::uint8_t {
    Software,
    VaapiToVulkan, // VA-API decode, frames imported into Vulkan through DMA-BUF
    VulkanVideo,   // Vulkan Video decode on OmaMovie's device
};

[[nodiscard]] std::string_view to_string(DecodePath path) noexcept;

inline constexpr std::size_t kMaxFrameImages = 4;

// The Vulkan images of a GPU frame while it is acquired (VideoFrame::acquire_gpu).
//
// Synchronization contract (FFmpeg's AVVkFrame): before touching image i, a submission waits on
// semaphores[i] reaching wait_values[i]; when done it signals signal_values[i]. Then the consumer
// calls GpuAccess::commit with the layout and access it left the images in.
struct GpuImages {
    std::array<VkImage, kMaxFrameImages> images{};
    std::array<VkFormat, kMaxFrameImages> formats{};
    std::array<VkImageLayout, kMaxFrameImages> layouts{};
    std::array<VkSemaphore, kMaxFrameImages> semaphores{};
    std::array<std::uint64_t, kMaxFrameImages> wait_values{};
    std::array<std::uint64_t, kMaxFrameImages> signal_values{};
    // One multi-planar image (Vulkan Video) or one image per plane (VA-API import).
    std::uint32_t image_count = 0;
};

class VideoFrame;

// Exclusive access to a GPU frame's images. Holds FFmpeg's frame lock until destroyed, so keep it
// short: record and submit, commit, release. The frame must outlive the access.
class GpuAccess {
public:
    GpuAccess(GpuAccess&& other) noexcept;
    GpuAccess& operator=(GpuAccess&& other) noexcept;
    GpuAccess(const GpuAccess&) = delete;
    GpuAccess& operator=(const GpuAccess&) = delete;
    ~GpuAccess();

    [[nodiscard]] const GpuImages& images() const noexcept { return images_; }

    // Records that a submission waiting on wait_values and signaling signal_values was made, and
    // the state it left the images in. Without a commit the frame's state is left untouched.
    void commit(VkImageLayout layout, VkAccessFlags2 access) noexcept;

private:
    friend class VideoFrame;
    explicit GpuAccess(void* frame, const GpuImages& images) noexcept;
    void release() noexcept;

    void* frame_ = nullptr; // the AVFrame; libs/media sources only
    GpuImages images_;
};

// One decoded picture. Move-only; holds a reference to FFmpeg's frame (and through it to the
// decoder's surface pool), so consumers release frames promptly.
class VideoFrame {
public:
    VideoFrame(VideoFrame&& other) noexcept;
    VideoFrame& operator=(VideoFrame&& other) noexcept;
    VideoFrame(const VideoFrame&) = delete;
    VideoFrame& operator=(const VideoFrame&) = delete;
    ~VideoFrame();

    [[nodiscard]] std::optional<RationalTime> pts() const noexcept;
    [[nodiscard]] std::optional<RationalTime> duration() const noexcept;
    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] DecodePath path() const noexcept;
    [[nodiscard]] bool on_gpu() const noexcept;

    // Memory layout of the samples, as an FFmpeg software format name ("nv12", "p010le",
    // "yuv420p"...). GPU frames report the layout of their images.
    [[nodiscard]] std::string_view pixel_format() const noexcept;
    [[nodiscard]] int bit_depth() const noexcept;
    [[nodiscard]] int plane_count() const noexcept;

    // Software frames only (empty on GPU frames): the bytes of a plane and its row stride.
    [[nodiscard]] std::span<const std::uint8_t> plane(int index) const noexcept;
    [[nodiscard]] int stride(int index) const noexcept;

    // GPU frames only: locks the frame and returns its images (Unsupported on software frames).
    [[nodiscard]] Result<GpuAccess> acquire_gpu();

private:
    friend class VideoDecoder;
    struct Impl;
    explicit VideoFrame(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::media
