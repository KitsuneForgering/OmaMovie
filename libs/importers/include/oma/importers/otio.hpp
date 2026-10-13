#pragma once

// OpenTimelineIO JSON (`.otio`) to ProjectIR (ADR-0008): an own reader for a pinned subset of
// the schemas (Timeline.1, Stack.1, Track.1, Clip.1/2, Gap.1, Transition.1, ExternalReference.1,
// MissingReference.1, RationalTime.1, TimeRange.1). Other objects are kept as opaque IR objects
// and reported. OTIO stores times as floating-point rates and values: exact NTSC rates are
// recognized, any other rounding is reported as a loss. Docs/interop.md has the support level.

#include "oma/base/error.hpp"
#include "oma/project_ir/ir.hpp"
#include "oma/project_ir/report.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace oma::importers {

struct Imported {
    ir::Timeline timeline;
    ir::ImportReport report;
};

// The file's text, at most 64 MiB, nesting at most 64 levels and 100,000 items.
[[nodiscard]] Result<Imported> import_otio(std::string_view json);

// A media target as a local path: a file:// URL with %XX escapes (a malformed escape is kept as
// text), or a path relative to `dir`, the folder of the OTIO file.
[[nodiscard]] std::filesystem::path otio_local_path(const std::string& target,
                                                    const std::filesystem::path& dir);

} // namespace oma::importers
