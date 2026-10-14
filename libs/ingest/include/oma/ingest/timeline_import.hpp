#pragma once

// An OpenTimelineIO file as a native project document (ADR-0008): the importer, then the converter
// with each media file probed by the library import's rule (media_entry). Probes and reads files:
// run it off the UI thread.

#include "oma/media/format.hpp"
#include "oma/project/document.hpp"
#include "oma/project_ir/report.hpp"

#include <filesystem>

namespace oma::ingest {

// `report` holds the import report as far as the import got, also when it fails.
[[nodiscard]] Result<project::Document> import_otio_file(const std::filesystem::path& path,
                                                         const media::MediaImporter& importer,
                                                         ir::ImportReport& report);

} // namespace oma::ingest
