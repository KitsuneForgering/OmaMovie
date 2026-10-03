#pragma once

#include "oma/media/probe.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace oma::media {

// Names are FFmpeg muxer and encoder names (for example mp4, libx264, aac). This describes
// an intended output; it does not imply that a timeline has been rendered or a file written.
struct ExportFormat {
    std::string container;
    std::string video_encoder;
    std::optional<std::string> audio_encoder;
};

// Input formats and output capability checks vary independently. Neither interface exposes
// FFmpeg types to the editor; both are internal, in-process extension points.
class MediaImporter {
public:
    virtual ~MediaImporter() = default;
    [[nodiscard]] virtual Result<MediaInfo>
    inspect_input(const std::filesystem::path& path) const = 0;
};

class ExportFormatChecker {
public:
    virtual ~ExportFormatChecker() = default;
    [[nodiscard]] virtual Result<void> check_output(const ExportFormat& format) const = 0;
};

class FfmpegFormatBackend final : public MediaImporter, public ExportFormatChecker {
public:
    [[nodiscard]] Result<MediaInfo> inspect_input(const std::filesystem::path& path) const override;
    [[nodiscard]] Result<void> check_output(const ExportFormat& format) const override;
};

} // namespace oma::media
