#pragma once

#include "oma/base/error.hpp"

#include <filesystem>
#include <optional>

// Publish staged outputs together. On an ordinary filesystem error, restore prior outputs.
// A failed rollback leaves backups in `stage` for manual recovery.
[[nodiscard]] oma::Result<void>
publish_export_bundle(const std::filesystem::path& stage, const std::filesystem::path& movie,
                      const std::filesystem::path& staged_movie,
                      const std::optional<std::filesystem::path>& sidecar,
                      const std::optional<std::filesystem::path>& staged_sidecar);
