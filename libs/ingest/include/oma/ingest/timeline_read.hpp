#pragma once

// An interchange file read to ProjectIR, as the app and `oma-project` both read it: sized before
// loading, an `.fcpxmld` bundle through its `Info.fcpxml`, the importer chosen by extension
// (importers::import_timeline). Reads a file: run it off the UI thread.

#include "oma/importers/otio.hpp"

#include <filesystem>

namespace oma::ingest {

struct TimelineRead {
    importers::Imported imported;
    std::filesystem::path dir; // the absolute folder the file's relative media paths start from
};

// An EDL names no frame rate: `edl_rate` is the one its timecodes count in.
[[nodiscard]] Result<TimelineRead> read_timeline(const std::filesystem::path& path,
                                                 Rational edl_rate);

} // namespace oma::ingest
