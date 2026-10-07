#pragma once

#include "viewer_frame.hpp"

#include "oma/base/error.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

// Export (M7): renders a timeline snapshot frame by frame through the viewer's own frame
// builder and compositor, mixes its sound with the playback mixer, and writes H.264 + AAC MP4
// (software encoders for now). The output replaces `file` only when everything succeeded.
struct ExportRequest {
    oma::timeline::Timeline timeline;
    MediaPaths paths;
    LutTables luts;
    std::filesystem::path file;
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;
    std::int64_t frames = 0;          // sequence frames [0, frames)
    std::int64_t ticks_per_frame = 1; // sequence ticks per frame
    oma::SampleRate audio_rate;
    // Auto: the GPU's encoder where OmaMovie has validated it (Intel VA-API), else software;
    // a failure to open it falls back to software and says why (ExportStats::encoder).
    enum class Encoder : std::uint8_t { Auto, Software, Hardware } encoder = Encoder::Auto;
};

// Where an export's time went (the --export-audit diagnostic), in milliseconds.
struct ExportStats {
    std::int64_t frames = 0;
    double build = 0;   // evaluation and decode (frame builder)
    double render = 0;  // composition and readback
    double convert = 0; // linear light to BT.709 RGB24
    double encode = 0;  // RGB to YUV and libx264
    double audio = 0;   // mix and AAC
    double total = 0;
    std::string encoder; // what was used, and why when it is not what was asked for
};

// Runs on a job worker. `progress(done, total)` returns false to cancel (the partial output is
// removed). Frames composite on a Vulkan device of the export's own: the viewer's is shared with
// Qt, which admits other submitters only while it draws a frame (ADR-0005), so an export there
// would stall whenever the window is hidden. Without Vulkan the CPU reference renders them.
[[nodiscard]] oma::Result<void> export_timeline(const ExportRequest& request,
                                                const std::function<bool(std::int64_t, std::int64_t)>& progress,
                                                ExportStats* stats = nullptr);

// Linear-light premultiplied RGBA (the compositor's output) to packed RGB24 with the BT.709
// transfer function, as the encoder expects. Exposed for the smoke check.
void encode_bt709(const float* rgba, std::size_t pixels, std::uint8_t* rgb);
