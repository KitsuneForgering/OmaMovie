#pragma once

#include "oma/base/error.hpp"

#include <filesystem>
#include <string>

namespace oma::ingest::detail {

// An interchange file's text, sized before reading: a file over importers::kMaxOtioBytes is
// refused without loading it, and a missing or unreadable one is an I/O error, not bad JSON.
[[nodiscard]] Result<std::string> read_timeline_text(const std::filesystem::path& path);

} // namespace oma::ingest::detail
