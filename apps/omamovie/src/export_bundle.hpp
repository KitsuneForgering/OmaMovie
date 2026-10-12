#pragma once

#include "exporter.hpp"

#include <optional>
#include <string>

// Render into a sibling staging directory, then publish the movie and optional SRT.
[[nodiscard]] oma::Result<void>
export_movie_bundle(ExportRequest request, std::optional<std::string> captions,
                    const std::function<bool(std::int64_t, std::int64_t)>& progress,
                    ExportStats* stats);
