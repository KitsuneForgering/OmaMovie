#pragma once

// The picture scopes measure: the frame at `frame` rendered at 256 × 144 by the CPU reference
// compositor from the viewer's render graph, display-encoded. Null when it cannot be built.
// Runs on the thread that owns `frames` (the session's job worker).

#include "viewer_frame.hpp"

#include <QImage>

[[nodiscard]] QImage small_frame(const oma::timeline::Timeline& timeline, const MediaPaths& paths,
                                 const LutTables& luts, std::int64_t frame, std::int64_t tpf,
                                 FrameSource& frames);
