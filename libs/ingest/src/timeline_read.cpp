#include "oma/ingest/timeline_read.hpp"

#include "oma/importers/timeline.hpp"

#include "timeline_text.hpp"

namespace oma::ingest {

Result<TimelineRead> read_timeline(const std::filesystem::path& path, Rational edl_rate) {
    auto text = detail::read_timeline_text(path);
    if (!text) {
        return std::unexpected(text.error());
    }
    auto imported = importers::import_timeline(*text, path.extension().string(), edl_rate);
    if (!imported) {
        return std::unexpected(imported.error());
    }
    // Relative paths resolve against the document: an `.fcpxmld` bundle holds its own.
    const auto absolute = std::filesystem::absolute(path);
    return TimelineRead{.imported = std::move(*imported),
                        .dir = path.extension() == ".fcpxmld" ? absolute : absolute.parent_path()};
}

} // namespace oma::ingest
