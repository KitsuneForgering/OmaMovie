#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/base/time.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Media probing (CLAUDE.md §6, §7): what a file contains, in OmaMovie's own types. FFmpeg types
// never appear in libs/media public headers.

namespace oma::media {

enum class StreamKind : std::uint8_t {
    Video,
    Audio,
    Subtitle,
    Data,
    Attachment,
    Unknown,
};

[[nodiscard]] std::string_view to_string(StreamKind kind) noexcept;

enum class ColorRange : std::uint8_t {
    Unspecified,
    Limited, // "TV" range
    Full,    // "PC" range
};

// Code points from ITU-T H.273, the values bitstreams, containers and FFmpeg share.
// 2 means "unspecified"; the compositor picks a default from the resolution (CLAUDE.md §10).
struct ColorInfo {
    std::uint8_t matrix = 2;
    std::uint8_t primaries = 2;
    std::uint8_t transfer = 2;
    ColorRange range = ColorRange::Unspecified;
};

struct VideoInfo {
    int width = 0; // coded size, before rotation
    int height = 0;
    std::string pixel_format; // FFmpeg name of the software layout, e.g. "yuv420p10le"
    int bit_depth = 8;
    ColorInfo color;
    // Counterclockwise rotation to apply for display (display matrix): 0, 90, 180 or 270.
    int rotation = 0;
    Rational sample_aspect = Rational::literal(1, 1);
    // Average rate. A nominal rate only: frames are located by timestamp (CLAUDE.md §6).
    std::optional<FrameRate> frame_rate;
    // Detected from the timestamps of the first packets: their spacing is not constant.
    bool variable_frame_rate = false;
    bool still_image = false;
};

struct AudioInfo {
    std::optional<SampleRate> sample_rate;
    int channels = 0;
    std::string channel_layout; // e.g. "stereo", "5.1"
    std::string sample_format;  // FFmpeg name, e.g. "fltp"
};

struct StreamInfo {
    int index = -1;
    StreamKind kind = StreamKind::Unknown;
    std::string codec;   // FFmpeg codec name, e.g. "h264"
    std::string profile; // e.g. "Main 10", empty if unknown
    Rational timebase = Rational::literal(1, 1);
    std::optional<RationalTime> start;
    std::optional<RationalTime> duration;
    std::optional<std::int64_t> frame_count; // from the container, when it records one
    std::string language;
    std::optional<VideoInfo> video;
    std::optional<AudioInfo> audio;
};

struct MediaInfo {
    std::string container; // FFmpeg demuxer name
    std::optional<RationalTime> duration;
    std::int64_t bit_rate = 0;
    std::vector<StreamInfo> streams;
    std::optional<int> best_video; // index into streams
    std::optional<int> best_audio;
};

// Opens the file, reads its headers and a few packets, and closes it again.
// Errors: IoError (missing/unreadable), InvalidData (corrupt or truncated), Unsupported.
[[nodiscard]] Result<MediaInfo> probe(const std::filesystem::path& path);

} // namespace oma::media
