#pragma once

// An interchange timeline as a native project document (ADR-0008): OTIO, CMX 3600 EDL or
// MLT/Kdenlive, by extension (importers::import_timeline); the importer, then the converter with
// each media file probed by the library import's rule (media_entry). Probes and reads files: run it
// off the UI thread.

#include "oma/media/format.hpp"
#include "oma/project/document.hpp"
#include "oma/project_ir/report.hpp"

#include <filesystem>

namespace oma::ingest {

// `report` holds the import report as far as the import got, also when it fails. An EDL names no
// frame rate: `edl_rate` is the one its timecodes count in.
[[nodiscard]] Result<project::Document>
import_timeline_file(const std::filesystem::path& path, const media::MediaImporter& importer,
                     ir::ImportReport& report, Rational edl_rate = Rational::literal(30, 1));

} // namespace oma::ingest
