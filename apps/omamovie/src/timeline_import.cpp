#include "timeline_import.hpp"

#include "oma/ingest/media_entry.hpp"

#include "oma/importers/otio.hpp"
#include "oma/project/from_ir.hpp"

#include <fstream>
#include <sstream>

std::optional<oma::project::Document> import_timeline(const std::filesystem::path& path,
                                                      const oma::media::MediaImporter& importer,
                                                      QStringList& issues, QString& why) {
    std::stringstream text;
    text << std::ifstream(path).rdbuf();
    auto imported = oma::importers::import_otio(text.str());
    if (!imported) {
        why = QString::fromStdString(imported.error().summary());
        return std::nullopt;
    }
    const auto dir = path.parent_path();
    const auto resolve = [&](const oma::ir::MediaRef& m) -> std::optional<oma::project::MediaRef> {
        const auto file = oma::importers::otio_local_path(m.target, dir);
        auto probed = importer.inspect_input(file);
        auto entry = probed ? oma::ingest::media_entry(*probed, {}) : std::nullopt;
        if (!entry)
            return std::nullopt;
        return oma::project::MediaRef{.info = entry->info,
                                      .path = file,
                                      .name = file.filename().string(),
                                      .audio_only = entry->audio_only,
                                      .fingerprint = oma::project::fingerprint_file(file).value_or(
                                          oma::project::Fingerprint{})};
    };
    auto doc = oma::project::from_ir(imported->timeline, resolve, imported->report);
    for (const auto& i : imported->report.issues)
        issues << QString::fromStdString(i.object + ": " + i.reason + "; " + i.impact);
    if (!doc)
        why = QString::fromStdString(doc.error().summary());
    return doc ? std::optional(std::move(*doc)) : std::nullopt;
}
