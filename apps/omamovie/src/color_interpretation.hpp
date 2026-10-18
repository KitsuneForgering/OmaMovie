#pragma once

// A media item's color interpretation (ADR-0019) applied to a decoded picture's declared color:
// Auto leaves what the stream says; the others replace the matrix or the range. RGB pictures are
// left alone.

#include "oma/media/probe.hpp"
#include "oma/timeline/model.hpp"

void interpret(oma::media::ColorInfo& color, const oma::timeline::MediaInfo& media);
