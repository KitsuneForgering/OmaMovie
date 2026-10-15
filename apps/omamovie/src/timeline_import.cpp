#include "timeline_import.hpp"

#include "oma/ingest/timeline_import.hpp"

std::optional<oma::project::Document> import_timeline(const std::filesystem::path& path,
                                                      const oma::media::MediaImporter& importer,
                                                      QStringList& issues, QString& why,
                                                      oma::Rational edl_rate) {
    oma::ir::ImportReport report;
    auto doc = oma::ingest::import_timeline_file(path, importer, report, edl_rate);
    for (const auto& i : report.issues)
        issues << QString::fromStdString(i.object + ": " + i.reason + "; " + i.impact);
    if (!doc)
        why = QString::fromStdString(doc.error().summary());
    return doc ? std::optional(std::move(*doc)) : std::nullopt;
}

std::optional<oma::Rational> edl_rate(const QString& text) {
    const QStringList parts = text.split('/');
    bool num_ok = false;
    bool den_ok = parts.size() == 1;
    const qint64 num = parts.value(0).toLongLong(&num_ok);
    const qint64 den = parts.size() == 2 ? parts[1].toLongLong(&den_ok) : 1;
    auto rate = num_ok && den_ok && parts.size() <= 2
                    ? oma::Rational::make(num, den)
                    : oma::Result<oma::Rational>(std::unexpected(
                          oma::Error(oma::ErrorCode::InvalidArgument, oma::Category::Ui, "rate")));
    return rate && rate->is_positive() ? std::optional(*rate) : std::nullopt;
}
