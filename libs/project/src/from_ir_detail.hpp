#pragma once

#include "oma/project/from_ir.hpp"
#include "oma/timeline/editor.hpp"

#include <string>
#include <unordered_map>

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
// The IR transition before `clip` as its transition in, or a reported hard cut.
void transition_into(timeline::Editor& editor, timeline::ClipId clip, const ir::Transition& t,
                     const std::string& who, ir::ImportReport& report);

} // namespace oma::project::from_ir_detail
