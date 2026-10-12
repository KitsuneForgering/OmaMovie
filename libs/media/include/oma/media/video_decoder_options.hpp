#pragma once

#include "oma/media/video_frame.hpp"

#include <optional>
#include <vector>

namespace oma::gpu {
class Device;
}

namespace oma::media {
struct VideoDecoderOptions {
    // OmaMovie's device; it must outlive the decoder. Without it only software decode is possible.
    const gpu::Device* device = nullptr;
    // Paths to try in order; omitting Software forbids that fallback.
    std::vector<DecodePath> paths = {DecodePath::VaapiToVulkan, DecodePath::VulkanVideo,
                                     DecodePath::Software};
    std::optional<int> stream; // stream index; default: the best video stream
    int threads = 0;           // software decode threads, 0 = automatic
};
} // namespace oma::media
