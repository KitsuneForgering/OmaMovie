#include "recordings.hpp"

#include <cstdlib>

namespace omarchy {
namespace {
std::filesystem::path env_dir(const char* name) {
    const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): UI thread
    return value != nullptr && *value != '\0' ? std::filesystem::path(value)
                                              : std::filesystem::path{};
}
} // namespace

std::filesystem::path recordings_dir() {
    if (auto dir = env_dir("OMARCHY_SCREENRECORD_DIR"); !dir.empty())
        return dir;
    if (auto dir = env_dir("XDG_VIDEOS_DIR"); !dir.empty())
        return dir;
    return env_dir("HOME") / "Videos";
}
} // namespace omarchy
