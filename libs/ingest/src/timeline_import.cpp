#include "oma/ingest/timeline_import.hpp"

#include "oma/importers/otio.hpp"
#include "oma/ingest/media_entry.hpp"
#include "oma/project/from_ir.hpp"

#include <fstream>
#include <sstream>

namespace oma::ingest {

Result<project::Document> import_otio_file(const std::filesystem::path& path,
                                           const media::MediaImporter& importer,
                                           ir::ImportReport& report) {
    std::stringstream text;
    text << std::ifstream(path).rdbuf();
    auto imported = importers::import_otio(text.str());
    if (!imported) {
        return std::unexpected(imported.error());
    }
    report = std::move(imported->report);
    const auto dir = std::filesystem::absolute(path).parent_path();
    const auto resolve = [&](const ir::MediaRef& m) -> std::optional<project::MediaRef> {
        const auto file = importers::otio_local_path(m.target, dir);
        auto probed = importer.inspect_input(file);
        auto entry = probed ? media_entry(*probed, {}) : std::nullopt;
        if (!entry) {
            return std::nullopt;
        }
        return project::MediaRef{
            .info = entry->info,
            .path = file,
            .name = file.filename().string(),
            .audio_only = entry->audio_only,
            .fingerprint = project::fingerprint_file(file).value_or(project::Fingerprint{})};
    };
    return project::from_ir(imported->timeline, resolve, report);
}

} // namespace oma::ingest
