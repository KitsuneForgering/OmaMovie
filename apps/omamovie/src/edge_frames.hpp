#pragma once

#include "viewer_frame.hpp"

#include "oma/timeline/model.hpp"

#include <array>
#include <optional>
#include <string>

// The two source frames a precise edit is judged by (a two-up display): for a roll, the
// outgoing clip's last frame and the incoming clip's first; for a slip, the clip's own first
// and last frames; for a slide, the last frame before it and the first frame after it.
enum class EdgeEdit { Roll, Slip, Slide };

struct EdgeSide {
    std::string path; // empty for a side without media (a title, or no neighbour)
    oma::RationalTime source;
    oma::timeline::MediaInfo media; // its color reading applies (ADR-0019)
};

[[nodiscard]] std::optional<std::array<EdgeSide, 2>>
edge_sides(const oma::timeline::Timeline& timeline, oma::timeline::ClipId clip, EdgeEdit edit,
           std::int64_t ticks_per_frame, const MediaPaths& paths);
