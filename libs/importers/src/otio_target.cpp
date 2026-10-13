#include "oma/importers/otio.hpp"

#include <charconv>
#include <string>

namespace oma::importers {

std::filesystem::path otio_local_path(const std::string& target, const std::filesystem::path& dir) {
    if (!target.starts_with("file://"))
        return dir / target;
    std::string path;
    for (std::size_t i = 7; i < target.size(); ++i) {
        unsigned char byte = 0;
        const char* hex = target.data() + i + 1;
        if (target[i] == '%' && i + 2 < target.size() &&
            std::from_chars(hex, hex + 2, byte, 16).ptr == hex + 2) {
            path += static_cast<char>(byte);
            i += 2;
        } else {
            path += target[i];
        }
    }
    return path;
}

} // namespace oma::importers
