#include "timeline_text.hpp"

#include "oma/importers/otio.hpp"

#include <fstream>
#include <system_error>

namespace oma::ingest::detail {

Result<std::string> read_timeline_text(const std::filesystem::path& bundle_or_file) {
    // An `.fcpxmld` is a folder whose document is `Info.fcpxml`.
    const auto path =
        bundle_or_file.extension() == ".fcpxmld" ? bundle_or_file / "Info.fcpxml" : bundle_or_file;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (!ec && size > importers::kMaxOtioBytes) {
        return make_error(ErrorCode::InvalidData, Category::Importer, "file larger than 64 MiB",
                          path.string());
    }
    std::string text(ec ? 0 : size, '\0');
    std::ifstream in(path, std::ios::binary);
    if (ec || !in.read(text.data(), static_cast<std::streamsize>(text.size()))) {
        return make_error(ErrorCode::IoError, Category::Importer, "cannot read the timeline file",
                          path.string());
    }
    return text;
}

} // namespace oma::ingest::detail
