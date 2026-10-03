#pragma once

#include "ffmpeg.hpp"

#include "oma/base/error.hpp"

namespace oma::gpu {
class Device;
}

namespace oma::media::ff {

// Wraps OmaMovie's VkDevice in an FFmpeg AVHWDeviceContext so FFmpeg decodes, maps and allocates
// on it instead of creating its own device (Docs/spikes/S2-ffmpeg-own-device.md). The Device
// must outlive the returned reference and everything derived from it.
[[nodiscard]] Result<BufferPtr> wrap_vulkan_device(const gpu::Device& device);

} // namespace oma::media::ff
