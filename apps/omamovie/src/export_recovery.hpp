#pragma once

#include "oma/base/error.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace export_recovery {

struct Record {
    std::filesystem::path stage;
    std::filesystem::path movie;
    std::optional<std::filesystem::path> sidecar;
    bool had_movie = false;
    bool had_sidecar = false;
};

enum class State { Staged, Interrupted, Completed };

[[nodiscard]] oma::Result<void> remember(const Record& record);
[[nodiscard]] std::vector<Record> pending();
[[nodiscard]] State state(const Record& record);
[[nodiscard]] oma::Result<void> restore_previous(const Record& record);
[[nodiscard]] oma::Result<void> keep_current(const Record& record);
void forget(const std::filesystem::path& stage);

} // namespace export_recovery
