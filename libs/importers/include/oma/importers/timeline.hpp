#pragma once

// The importer for an interchange file, chosen by its extension: `.edl` (CMX 3600, at
// `edl_rate`), `.mlt` and `.kdenlive` (MLT XML), `.fcpxml` and `.fcpxmld` (the bundle's
// `Info.fcpxml`), anything else OpenTimelineIO JSON.

#include "oma/importers/otio.hpp"

#include <string_view>

namespace oma::importers {

[[nodiscard]] Result<Imported> import_timeline(std::string_view text, std::string_view extension,
                                               Rational edl_rate);

} // namespace oma::importers
