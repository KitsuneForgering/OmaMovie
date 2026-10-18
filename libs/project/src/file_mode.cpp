#include "file_mode.hpp"

#include <sys/stat.h>

#include <fstream>
#include <string>

namespace oma::project::detail {

namespace {

// umask(2) can only be read by setting it, which races with other threads creating files;
// Linux reports it in /proc/self/status instead ("Umask:\t0022").
mode_t process_umask() {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        if (line.starts_with("Umask:")) {
            try {
                return static_cast<mode_t>(std::stoul(line.substr(6), nullptr, 8) & 0777U);
            } catch (...) {
                break;
            }
        }
    }
    return 022; // the usual default when the kernel does not say
}

} // namespace

mode_t saved_file_mode(const std::filesystem::path& file) {
    struct stat existing{};
    if (::stat(file.c_str(), &existing) == 0) {
        return existing.st_mode & 07777;
    }
    return 0666 & ~process_umask();
}

} // namespace oma::project::detail
