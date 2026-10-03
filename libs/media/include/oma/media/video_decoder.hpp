#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"
#include "oma/media/probe.hpp"
#include "oma/media/video_frame.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace oma::gpu {
class Device;
}

namespace oma::media {

struct VideoDecoderOptions {
    // OmaMovie's device; it must outlive the decoder. Without it only software decode is possible.
    const gpu::Device* device = nullptr;
    // Paths to try, in order. The default is the policy measured in S1/S2: VA-API imported into
    // Vulkan, then Vulkan Video, then software. Leave Software out to forbid the fallback.
    std::vector<DecodePath> paths = {DecodePath::VaapiToVulkan, DecodePath::VulkanVideo,
                                     DecodePath::Software};
    std::optional<int> stream; // stream index; default: the best video stream
    int threads = 0;           // software decode threads, 0 = automatic
};

// Decodes one video stream of a file in presentation order. Not thread-safe: one thread drives a
// decoder (the playback scheduler owns one per clip).
//
// A hardware path is chosen at open, but the driver only confirms it on the first frame (profile,
// size and level limits). If it refuses, the decoder reopens in software, logs the downgrade and
// path() changes; the frames delivered are the same either way.
class VideoDecoder {
public:
    [[nodiscard]] static Result<std::unique_ptr<VideoDecoder>>
    open(const std::filesystem::path& path, const VideoDecoderOptions& options = {});

    ~VideoDecoder();
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoDecoder(VideoDecoder&&) = delete;
    VideoDecoder& operator=(VideoDecoder&&) = delete;

    [[nodiscard]] const StreamInfo& stream() const noexcept;
    [[nodiscard]] DecodePath path() const noexcept;

    // The next frame, or std::nullopt at the end of the stream.
    [[nodiscard]] Result<std::optional<VideoFrame>> next();

    // Makes the next frame the one on screen at `t`: the last frame whose pts <= t, or the first
    // frame when `t` precedes it. Exact on variable frame rate media (CLAUDE.md §6).
    [[nodiscard]] Result<void> seek(const RationalTime& t);

private:
    struct Impl;
    explicit VideoDecoder(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::media
