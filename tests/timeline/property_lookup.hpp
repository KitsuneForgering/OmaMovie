#pragma once

// Positional lookups for property steps (property_ops.hpp) and the clips they place.

#include "oma/timeline/edit.hpp"
#include "oma/timeline/model.hpp"
#include "property_ops.hpp"

#include <cstdint>

namespace property {

[[nodiscard]] const oma::timeline::Track* track_at(const oma::timeline::Timeline& tl,
                                                   std::int64_t index);
[[nodiscard]] const oma::timeline::Clip* clip_at(const oma::timeline::Timeline& tl,
                                                 std::int64_t track, std::int64_t index);
// The ID at a position, or an invalid ID the edit rejects.
[[nodiscard]] oma::timeline::TrackId track_id(const oma::timeline::Timeline& tl,
                                              std::int64_t index);
[[nodiscard]] oma::timeline::ClipId clip_id(const oma::timeline::Timeline& tl, std::int64_t track,
                                            std::int64_t index);
// A speed ramp over the whole clip; null clip gives an invalid ID the edit rejects.
[[nodiscard]] std::unique_ptr<oma::timeline::Command> ramp(const Op& op,
                                                           const oma::timeline::Clip* clip);
// Append, Insert or Overwrite. Media 0 is the camera (300 frames), 1 the music (60 s at 48 kHz),
// 2 the still picture.
[[nodiscard]] std::unique_ptr<oma::timeline::Command> placement(const Op& op,
                                                                oma::timeline::Editor& editor);

} // namespace property
