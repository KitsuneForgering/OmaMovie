#include "viewer_decoder.hpp"

#include "oma/gpu/device.hpp"

oma::Result<std::shared_ptr<ViewerFrame>>
decode_paused_frame(FrameSource& frames, const oma::gpu::Device* device, const std::function<bool()>& wanted,
                    const oma::timeline::Timeline& timeline, const MediaPaths& paths, const LutTables& luts,
                    std::uint32_t width, std::uint32_t height, std::int64_t frame, std::int64_t ticks_per_frame) {
    const auto lost = [] {
        return std::unexpected(oma::Error(oma::ErrorCode::DeviceLost, oma::Category::Decode, "GPU device lost"));
    };
    if (device != nullptr && device->lost()) return lost();
    const bool admitted = device != nullptr && device->supports_video_decode();
    if (admitted && !device->admit([&] { return !wanted(); })) {
        if (device->lost()) return lost();
        return std::unexpected(oma::Error(oma::ErrorCode::Cancelled, oma::Category::Decode, "superseded"));
    }
    frames.set_interrupt([&] { return !wanted(); });
    auto view = build_viewer_frame(timeline, paths, luts, width, height, frame, ticks_per_frame, frames);
    frames.set_interrupt({});
    if (admitted) device->leave();
    if (!view && device != nullptr && device->lost()) return lost();
    return view;
}
