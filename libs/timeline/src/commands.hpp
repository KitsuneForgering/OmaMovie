#pragma once

#include "oma/timeline/editor.hpp"
#include "oma/timeline/model.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Internal building blocks. Primitive commands each change one thing and remember exactly what
// they changed, so revert restores the prior state bit for bit. Editing operations are planned
// commands: on their first apply they read the timeline, plan a list of primitives and apply
// them as one transaction; redo re-applies the same primitives (the state is identical).

namespace oma::timeline::detail {

using Steps = std::vector<std::unique_ptr<Command>>;
using Planner = std::function<Result<Steps>(Timeline&)>;

[[nodiscard]] std::unique_ptr<Command> make_transaction(std::string name, Steps steps);
[[nodiscard]] std::unique_ptr<Command> make_planned(std::string name, Planner planner);

[[nodiscard]] std::unique_ptr<Command> insert_clip(TrackId track, Clip clip);
[[nodiscard]] std::unique_ptr<Command> erase_clip(ClipId id);
// Replaces the clip with the same ID (any field, including its position on the track).
[[nodiscard]] std::unique_ptr<Command> replace_clip(Clip clip);
// Moves every clip of the track starting at or after `from` by `delta` ticks.
[[nodiscard]] std::unique_ptr<Command> shift_clips(TrackId track, std::int64_t from,
                                                   std::int64_t delta);
[[nodiscard]] std::unique_ptr<Command> insert_track(Track track, std::size_t index);
[[nodiscard]] std::unique_ptr<Command> erase_track(TrackId id);
[[nodiscard]] std::unique_ptr<Command> set_flags(TrackId id, bool muted, bool hidden);
[[nodiscard]] std::unique_ptr<Command> insert_marker(Marker marker);
[[nodiscard]] std::unique_ptr<Command> erase_marker(MarkerId id);

} // namespace oma::timeline::detail
