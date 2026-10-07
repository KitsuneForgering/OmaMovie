#pragma once

#include "commands.hpp"

#include "oma/timeline/model.hpp"

#include <unordered_map>

namespace oma::timeline::detail {

// Where a clip was before an edit: enough to tell what the edit changed (ADR-0014).
struct ClipPlace {
    TrackId track;
    MediaId media;
    std::int64_t start = 0;
    std::int64_t duration = 0;
    RationalTime source_in;
};
using Places = std::unordered_map<ClipId, ClipPlace>;

[[nodiscard]] bool has_anchors(const Timeline& tl);
[[nodiscard]] Places places(const Timeline& tl);
// The steps that re-attach connected clips after an edit (ADR-0014), given where every clip
// was before it. Empty when nothing needs to change.
[[nodiscard]] Result<Steps> reattach(const Timeline& tl, const Places& before);

} // namespace oma::timeline::detail
