#pragma once

#include "oma/base/error.hpp"
#include "oma/base/time.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace oma::media {

// Sequential SDR video output. Input is packed RGB24 with BT.709 primaries and transfer;
// the writer converts it to limited-range 4:2:0 for H.264 in MP4. With `audio`, an AAC track
// takes interleaved float samples at that rate. The output is replaced only after finish()
// succeeds. A discarded writer removes its temporary file.
struct AudioTrack {
    SampleRate rate;
    int channels = 2; // 1 or 2
};

// Software is libx264 (CPU). VaApi is the GPU's fixed-function H.264 encoder through VA-API:
// the same resolution, rate, colour tags and container, at a quality of its own.
enum class VideoEncoder : std::uint8_t { Software, VaApi };

struct VideoWriterOptions {
    std::optional<AudioTrack> audio;
    VideoEncoder encoder = VideoEncoder::Software;
    std::filesystem::path render_node; // VA-API device; empty: libva's default
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
