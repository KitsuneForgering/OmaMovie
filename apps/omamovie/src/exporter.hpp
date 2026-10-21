#pragma once

#include "viewer_frame.hpp"

#include "oma/base/error.hpp"
#include "oma/media/video_writer.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

// Export (M7): renders a timeline snapshot frame by frame through the viewer's own frame
// builder and compositor, mixes its sound with the playback mixer, and writes the chosen
// container and codec profile. The output replaces `file` only when everything succeeded.
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
    // Auto: hardware decode where OmaMovie has validated it (Intel VA-API, as the viewer), else
    // software; a stream whose hardware decoder cannot open falls back to software per file.
    enum class Decode : std::uint8_t { Auto, Software, Hardware } decode = Decode::Auto;
    // GPU-native encoder input (proposed ADR-0023): with the VA-API encoder and a GPU
    // compositor, frames are written on the GPU into the encoder's own surfaces instead of read
    // back and uploaded. Off until the ADR is accepted; ExportStats::encoder_input says the route.
    bool gpu_input = false;
    oma::media::OutputProfile profile = oma::media::OutputProfile::Mp4H264Aac;
};

// Where an export's time went (the --export-audit diagnostic), in milliseconds.
struct ExportStats {
    std::int64_t frames = 0;
    double build = 0;   // evaluation and decode (frame builder)
    double render = 0;  // composition and readback (or the CPU reference and BT.709 RGB24)
    double convert = 0; // waiting for the encoder stage (backpressure)
    double encode = 0;  // RGB to YUV, video and audio encoders, on the encoder stage's worker
    double audio = 0;   // mix
    double total = 0;
    double process_cpu_ms = 0;   // all process threads while this export job runs
    std::string decode_path;     // export frame source and its hardware choice
    std::string compositor_path; // selected renderer and any downgrade reason
    std::string encoder;         // what was used, and why when it is not what was asked for
    std::string encoder_input;   // how frames reach the encoder (readback, or GPU-native and why not)
    std::uint64_t surface_frames = 0; // frames written on the GPU into encoder surfaces
    // Transfer audit: counts at OmaMovie's explicit API boundaries, not driver-internal copies.
    FrameSource::DecodeTransferStats decoded;
    std::uint64_t software_inputs = 0;
    std::uint64_t software_input_bytes = 0; // plane payload exposed to the compositor
    std::uint64_t staged_inputs = 0;
    std::uint64_t staged_bytes = 0; // packed software rows copied into Vulkan upload memory
    double staging_ms = 0;
    double staging_row_copy_ms = 0;
    std::uint64_t readbacks = 0;
    std::uint64_t readback_bytes = 0;
    double readback_ms = 0; // full readback call, including setup and host copy
    double nv12_record_ms = 0;
    double nv12_submit_ms = 0;
    double nv12_fence_wait_ms = 0; // queued work and GPU execution, not shader-only time
    double nv12_gpu_convert_ms = 0;
    std::uint64_t nv12_gpu_timestamp_frames = 0;
    double nv12_invalidate_ms = 0;
    double nv12_host_copy_ms = 0;
    oma::media::WriterTransferStats writer_transfers;
};

// Runs on a job worker. `progress(done, total)` returns false to cancel (the partial output is
// removed). Frames composite on a Vulkan device of the export's own: the viewer's is shared with
// Qt, which admits other submitters only while it draws a frame (ADR-0005), so an export there
// would stall whenever the window is hidden. Without Vulkan the CPU reference renders them.
[[nodiscard]] oma::Result<void>
export_timeline(const ExportRequest& request,
                const std::function<bool(std::int64_t, std::int64_t)>& progress,
                ExportStats* stats = nullptr);

namespace oma::gpu {
class Device;
}

// The same export on a caller's Vulkan device (nullptr: the CPU reference), which must outlive the
// call. If the device is lost mid-export, the remaining frames render on the CPU reference.
[[nodiscard]] oma::Result<void>
export_timeline_on(const ExportRequest& request, oma::gpu::Device* device,
                   const std::function<bool(std::int64_t, std::int64_t)>& progress,
                   ExportStats* stats = nullptr);

// Linear-light premultiplied RGBA (the compositor's output) to packed RGB24 with the BT.709
// transfer function, as the encoder expects. Exposed for the smoke check.
void encode_bt709(const float* rgba, std::size_t pixels, std::uint8_t* rgb);
