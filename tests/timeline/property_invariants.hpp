#pragma once

// Invariants checked after every successful step, written apart from Timeline::validate() so
// they are an independent oracle (docs/implementation-plan-qa-timeline.md). Each returns an
// empty string when the state holds, or what broke.

#include "oma/timeline/model.hpp"

#include <string>

namespace property {

// Track and clip IDs unique; clips sorted, apart, positive and on the sequence grid; media of a
// kind the track plays; anchors to an unanchored clip on another track; markers on the grid.
[[nodiscard]] std::string broken_structure(const oma::timeline::Timeline& tl);
// A segmented map covers its clip exactly, and the media the clip shows lies inside the media.
[[nodiscard]] std::string broken_extent(const oma::timeline::Timeline& tl,
                                        const oma::timeline::Clip& clip);

} // namespace property
