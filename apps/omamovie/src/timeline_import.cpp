#include "timeline_import.hpp"

#include "oma/ingest/timeline_import.hpp"

std::optional<oma::project::Document> import_timeline(const std::filesystem::path& path,
                                                      const oma::media::MediaImporter& importer,
                                                      QStringList& issues, QString& why) {
    oma::ir::ImportReport report;
    auto doc = oma::ingest::import_timeline_file(path, importer, report);
    for (const auto& i : report.issues)
        issues << QString::fromStdString(i.object + ": " + i.reason + "; " + i.impact);
    if (!doc)
        why = QString::fromStdString(doc.error().summary());
    return doc ? std::optional(std::move(*doc)) : std::nullopt;
}
