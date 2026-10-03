#pragma once

#include "ffmpeg.hpp"

#include "oma/media/probe.hpp"

#include <string_view>

namespace oma::media {

// Describes one stream of an opened file (shared by probe and the decoders).
[[nodiscard]] StreamInfo describe_stream(const AVStream& st, std::string_view demuxer);

} // namespace oma::media
