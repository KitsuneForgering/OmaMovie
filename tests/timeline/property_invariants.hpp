#pragma once

// Invariants checked after every successful step, written apart from Timeline::validate() so
// they are an independent oracle (Docs/implementation-plan-qa-timeline.md). Each returns an
// empty string when the state holds, or what broke.

#include "oma/timeline/model.hpp"

#include <string>

namespace property {

// Track and clip IDs unique; clips sorted, apart, positive and on the sequence grid; media of a
// kind the track plays; anchors to an unanchored clip on another track; valid transitions;
// broken_video and broken_audio; then broken_annotations.
[[nodiscard]] std::string broken_structure(const oma::timeline::Timeline& tl);
// Marker IDs valid and unique, on the grid, not before the start, in time order; caption IDs
// valid and unique, on the grid, not before the start, not empty, within kMaxCaptionBytes,
// sorted and apart.
[[nodiscard]] std::string broken_annotations(const oma::timeline::Timeline& tl);
// Effects unique and at most kMaxEffects, parameters unique and finite (sepia's amount in
// [0, 1]); opacity keys at most kMaxKeys, in [0, 1], strictly increasing; transform keys strictly
// increasing with finite, non-zero scales.
[[nodiscard]] std::string broken_video(const oma::timeline::Clip& clip);
// Gain keys in [0, 16] and strictly increasing; fades not negative and together within the clip;
// equalizer bands within ±24 dB.
[[nodiscard]] std::string broken_audio(const oma::timeline::Clip& clip);
// A segmented map covers its clip exactly, and the media the clip shows lies inside the media.
[[nodiscard]] std::string broken_extent(const oma::timeline::Timeline& tl,
                                        const oma::timeline::Clip& clip);

} // namespace property
