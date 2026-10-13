#pragma once

// ProjectIR to a native project (ADR-0008): the converter, the one place where an import meets
// OmaMovie's model, so importers never touch it (CLAUDE.md §16.2).

#include "oma/project/document.hpp"
#include "oma/project_ir/ir.hpp"
#include "oma/project_ir/report.hpp"

#include <functional>
#include <optional>

namespace oma::project {

// What the app found for an IR media item by probing its file (this library cannot); the
// converter assigns `info.id`. Nullopt when the file cannot be found or read.
using ResolveMedia = std::function<std::optional<MediaRef>(const ir::MediaRef&)>;

// A project from `timeline`: the first video track becomes the storyline, the others lanes;
// clips keep their place, gaps stay as space and dissolves become the next clip's transition.
// A clip whose media cannot be resolved or placed leaves a gap of its length. Everything
// approximated, replaced or not carried into the project is added to `report`.
[[nodiscard]] Result<Document> from_ir(const ir::Timeline& timeline, const ResolveMedia& resolve,
                                       ir::ImportReport& report);

} // namespace oma::project
