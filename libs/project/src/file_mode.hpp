#pragma once

#include <sys/types.h>

#include <filesystem>

namespace oma::project::detail {

// The permission bits a save gives `file`: the existing file's, so a project the user
// restricted stays restricted, else 0666 masked by the process umask, as a new file
// created by open(2) would get. The temporary file mkstemp makes is 0600 either way.
[[nodiscard]] mode_t saved_file_mode(const std::filesystem::path& file);

} // namespace oma::project::detail
