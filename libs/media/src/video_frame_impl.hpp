#pragma once

#include "ffmpeg.hpp"

#include "oma/media/video_frame.hpp"

#include <optional>

namespace oma::media {

struct VideoFrame::Impl {
    ff::FramePtr frame; // AV_PIX_FMT_VULKAN on GPU paths, a software format otherwise
    DecodePath path = DecodePath::Software;
    AVPixelFormat layout = AV_PIX_FMT_NONE; // software format of the samples
    std::optional<RationalTime> pts;
    std::optional<RationalTime> duration;
};

} // namespace oma::media
