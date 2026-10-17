#pragma once

#include "frame_source.hpp"

#include "oma/base/error.hpp"
#include "oma/compositor/render_graph.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Media ID -> file, as the library knows it.
using MediaPaths = std::unordered_map<std::uint64_t, std::string>;
// LUT ID -> its table (ADR-0012). Shared immutable: replaced as a whole, read by any thread.
using LutTables = std::unordered_map<std::uint64_t, std::shared_ptr<const oma::compositor::Lut3d>>;

// What the viewer shows at one instant: the render graph built from the timeline and one
// decoded picture per layer input.
struct ViewerFrame {
    oma::compositor::RenderGraph graph;
    std::vector<Picture> pictures;
    std::int64_t frame = 0; // sequence frame
    double seconds = 0;     // its time, for display and measurements only
};

// Builds the frame at sequence frame `frame`: evaluates the timeline (every visible layer,
// bottom first, with its fit/crop/transform/opacity/blend; a gap is the black background) and
// decodes each layer's picture with `frames`. Runs on the thread that owns `frames`.
[[nodiscard]] oma::Result<std::shared_ptr<ViewerFrame>>
build_viewer_frame(const oma::timeline::Timeline& timeline, const MediaPaths& paths, const LutTables& luts,
                   std::uint32_t width, std::uint32_t height, std::int64_t frame, std::int64_t ticks_per_frame,
                   FrameSource& frames);
