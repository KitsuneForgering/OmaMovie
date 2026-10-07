#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// Omarchy screen recordings (Docs/Research/omarchy-integration.md §5). The rules mirror the
// installed `omarchy-capture-screenrecording`, which is version-sensitive: keep them here, in
// the app's platform adapter, and fall back to plain defaults.
namespace omarchy {

// ${OMARCHY_SCREENRECORD_DIR:-${XDG_VIDEOS_DIR:-$HOME/Videos}}, as the capture script picks it.
[[nodiscard]] std::filesystem::path recordings_dir();

struct Recording {
    std::filesystem::path path;
    std::filesystem::file_time_type modified;
    std::uintmax_t size = 0;
    // Still being written: the capture script names it as the current recording and it
    // changed within the last few seconds. Not offered for editing until it settles.
    bool growing = false;
};

// Video files in `dir`, newest first, at most `limit`. Skips the script's "-preview.png" files
// and anything that is not a regular video file.
[[nodiscard]] std::vector<Recording> list_recordings(const std::filesystem::path& dir,
                                                     std::size_t limit);

} // namespace omarchy
