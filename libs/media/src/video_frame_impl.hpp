#pragma once

#include "ffmpeg.hpp"

#include "oma/media/video_frame.hpp"

#include "oma/gpu/resources.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace oma::media {

// A VA-API surface imported into Vulkan by OmaMovie (ADR-0004, 2026-10-06): one image per plane
// over the surface's DMA-BUFs, and one timeline semaphore per image for the GpuImages contract.
// Destroyed before the DRM mapping that keeps the surface and its descriptors alive.
struct ImportedFrame {
    const gpu::Device* device = nullptr;
    std::array<gpu::DmaBufImage, kMaxFrameImages> images;
    std::array<VkSemaphore, kMaxFrameImages> semaphores{};
    std::array<std::uint64_t, kMaxFrameImages> values{}; // last value signaled by a consumer
    std::uint32_t count = 0;
    std::mutex lock; // held by a GpuAccess, like FFmpeg's lock_frame

    ImportedFrame() = default;
    ImportedFrame(const ImportedFrame&) = delete;
    ImportedFrame& operator=(const ImportedFrame&) = delete;
    ImportedFrame(ImportedFrame&&) = delete;
    ImportedFrame& operator=(ImportedFrame&&) = delete;
    ~ImportedFrame(); // waits for the consumers' last signals before destroying
};

struct VideoFrame::Impl {
    // AV_PIX_FMT_VULKAN for Vulkan Video, AV_PIX_FMT_DRM_PRIME for imported VA-API frames, a
    // software format otherwise.
    ff::FramePtr frame;
    DecodePath path = DecodePath::Software;
    AVPixelFormat layout = AV_PIX_FMT_NONE; // software format of the samples
    std::optional<RationalTime> pts;
    std::optional<RationalTime> duration;
    std::unique_ptr<ImportedFrame> imported; // VA-API path; destroyed before `frame`
};

} // namespace oma::media
