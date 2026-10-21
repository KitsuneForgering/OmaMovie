#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"
#include "oma/media/encoder_surfaces.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace oma::media {

// Sequential SDR video output. Input is packed RGB24 with BT.709 primaries and transfer;
// the writer converts it to limited-range 4:2:0 for the selected video codec. With `audio`,
// the selected audio codec takes interleaved float samples at that rate. The output is replaced
// only after finish() succeeds. A discarded writer removes its temporary file.
struct AudioTrack {
    SampleRate rate;
    int channels = 2; // 1 or 2
};

// VaApi is the GPU's fixed-function H.264 encoder through VA-API. Other profiles use their
// software encoder; the same resolution, rate and colour tags are kept.
enum class VideoEncoder : std::uint8_t { Software, VaApi };

// Output combinations implemented by the writer. A format check alone does not make an
// arbitrary encoder usable: sample layout, pixel format and timestamp rules also matter.
enum class OutputProfile : std::uint8_t {
    Mp4H264Aac,
    MatroskaH264Aac,
    MatroskaHevcAac,
    WebmVp9Vorbis,
};

struct VideoWriterOptions {
    std::optional<AudioTrack> audio;
    VideoEncoder encoder = VideoEncoder::Software;
    std::filesystem::path render_node; // VA-API device; empty: libva's default
    OutputProfile profile = OutputProfile::Mp4H264Aac;
};

// Diagnostic counters for CPU input to the hardware encoder. Read after the writer's
// caller has joined its encoding worker; these are not synchronized for live polling.
struct WriterTransferStats {
    std::uint64_t nv12_copy_bytes = 0;
    std::uint64_t va_upload_bytes = 0;
    std::uint64_t va_uploads = 0;
    double nv12_copy_ms = 0;
    double va_upload_ms = 0;
};

class VideoWriter {
public:
    [[nodiscard]] static Result<std::unique_ptr<VideoWriter>>
    create(const std::filesystem::path& file, int width, int height, FrameRate rate,
           const VideoWriterOptions& options = {});

    ~VideoWriter();
    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;

    [[nodiscard]] Result<void> write(std::span<const std::uint8_t> rgb, int stride);
    // The same from packed RGBA8 (alpha ignored), as a GPU readback delivers it.
    [[nodiscard]] Result<void> write_rgba(std::span<const std::uint8_t> rgba, int stride);
    // Whether write_nv12 applies: the VA-API encoder, which takes NV12 surfaces.
    [[nodiscard]] bool takes_nv12() const noexcept;
    // An NV12 frame (BT.709 limited range): `stride`-byte luma rows, then interleaved CbCr rows
    // at half height (compositor::VulkanCompositor::read_nv12). No conversion on the CPU.
    [[nodiscard]] Result<void> write_nv12(std::span<const std::uint8_t> nv12, int stride);
    // GPU-native input (ADR-0023): the VA-API encoder's surface pool with planes writable on
    // `device`. Unsupported for other encoders or a device without DMA-BUF import; the caller
    // then keeps write_nv12. The result must be destroyed before `device`.
    [[nodiscard]] Result<std::unique_ptr<EncoderSurfaces>> surfaces(const gpu::Device& device);
    // Encodes an acquired surface whose planes the caller has filled and synchronized (the
    // device's work on them finished and released to the foreign queue family), in order with
    // the other video frames.
    [[nodiscard]] Result<void> write_surface(const EncoderSurface& surface);
    [[nodiscard]] WriterTransferStats transfer_stats() const noexcept;
    // Appends interleaved samples (channels per frame) to the audio track, in order. The
    // caller keeps audio and video in step (the muxer interleaves by timestamp).
    [[nodiscard]] Result<void> write_audio(std::span<const float> interleaved);
    [[nodiscard]] Result<void> finish();

private:
    struct Impl;
    explicit VideoWriter(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::media
