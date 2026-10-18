#pragma once

#include "oma/base/error.hpp"
#include "oma/media/video_frame.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>

// Draws a title clip's text (ADR-0015) at the canvas size into a straight-alpha GBRA frame, the
// picture the compositor blends like any other layer. Qt's text engine does the shaping; any
// thread may call it (QImage painting with fontconfig, the app font read under Qt's lock).
//
// Every pixel carries the text colour and only alpha varies, so bilinear filtering at glyph
// edges blends with the text colour rather than with black.
[[nodiscard]] oma::Result<oma::media::VideoFrame> rasterize_title(const oma::timeline::Title& title,
                                                                  std::uint32_t width, std::uint32_t height);
