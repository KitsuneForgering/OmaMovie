#pragma once

#include "frame_source.hpp"
#include "viewer_frame.hpp"

#include "oma/base/error.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace oma::gpu {
class Device;
}

// Decodes the paused viewer's frame on a job worker, so Qt's sync phase only composites
// (async interactive work, ADR-0020). `frames` decodes in hardware when it was made with
// `device`: VA-API decoding and OmaMovie's DMA-BUF import submit nothing to the graphics queue,
// so they run without admission and never make Qt's frame end wait for a seek. Vulkan Video
// decoding does submit, so a device that can do it is admitted around the decode (ADR-0005).
//
// Fails with ErrorCode::Cancelled once `wanted` returns false (checked between decoded frames,
// so a superseded seek stops within one frame) and with ErrorCode::DeviceLost when the device is
// or becomes lost; neither is a session failure. Runs on the thread that owns `frames`.
[[nodiscard]] oma::Result<std::shared_ptr<ViewerFrame>>
decode_paused_frame(FrameSource& frames, const oma::gpu::Device* device, const std::function<bool()>& wanted,
                    const oma::timeline::Timeline& timeline, const MediaPaths& paths, const LutTables& luts,
                    std::uint32_t width, std::uint32_t height, std::int64_t frame, std::int64_t ticks_per_frame);
