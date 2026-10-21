#pragma once

#include "oma/project/from_ir.hpp"
#include "oma/timeline/editor.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace oma::project::from_ir_detail {

using MediaIds = std::unordered_map<std::string, timeline::MediaId>;

// Places one IR track's items on `track` from sequence time zero, reporting what changes.
[[nodiscard]] Result<void> place_items(timeline::Editor& editor, timeline::TrackId track,
                                       const ir::Track& source, const MediaIds& media,
                                       ir::ImportReport& report);

void issue(ir::ImportReport& report, std::string object, std::string reason, std::string impact);

// Library items for the IR media the resolver finds, with their new ids in `ids`.
void resolve_media(timeline::Editor& editor, const ir::Timeline& timeline,
                   const ResolveMedia& resolve, Document& doc, MediaIds& ids,
                   ir::ImportReport& report);
// Reports how many objects the import kept as opaque (CLAUDE.md §16.3) the project cannot save.
void report_unsaved(const ir::Timeline& timeline, ir::ImportReport& report);
// Sequence ticks for a source length, reporting when it is not on the sequence grid.
[[nodiscard]] std::int64_t ticks_of(const timeline::Timeline& t, const RationalTime& length,
                                    const std::string& who, ir::ImportReport& report);
// The sequence time a clip lasts: its source range at its speed (MLT's timewarp frames).
[[nodiscard]] RationalTime played(const ir::Clip& clip);
// One edit to make, made again if its group has to be retried.
using Edit = std::function<std::unique_ptr<timeline::Command>()>;
// Executes `edits` in order; landed[i] says whether edit i took effect. A group runs as one
// transaction (one timeline-wide validation, not one per edit, which made an import of n clips
// O(n^2)); a group that fails is halved until each edit lands or fails on its own, as it would
// executed alone.
void execute_each(timeline::Editor& editor, std::span<const Edit> edits,
                  std::span<std::uint8_t> landed);

} // namespace oma::project::from_ir_detail
