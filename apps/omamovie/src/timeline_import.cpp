#include "timeline_import.hpp"

#include "oma/ingest/timeline_import.hpp"
#include "oma/playback/timeline_audio.hpp"

#include <algorithm>

namespace {
// Clips with sound whose speed the mixer cannot follow (playback::retimed_rate, its own rule).
void note_silent_speeds(const oma::project::Document& doc, oma::SampleRate mixer_rate,
                        QStringList& issues) {
    if (!doc.timeline)
        return;
    for (const auto& track : doc.timeline->tracks())
        for (const auto& c : track.clips) {
            const auto media =
                std::ranges::find(doc.media, c.media, [](const auto& m) { return m.info.id; });
            const oma::Rational speed = c.time_map.speed();
            if (media != doc.media.end() && media->info.has_audio && c.time_map.is_constant() &&
                speed != oma::Rational::literal(1, 1) &&
                !oma::playback::retimed_rate(c, mixer_rate))
                issues << QStringLiteral(
                              "%1: speed %2/%3 plays without sound; the picture keeps it")
                              .arg(QString::fromStdString(media->name))
                              .arg(speed.num())
                              .arg(speed.den());
        }
}
} // namespace

std::optional<oma::project::Document> import_timeline(const std::filesystem::path& path,
                                                      const oma::media::MediaImporter& importer,
                                                      QStringList& issues, QString& why,
                                                      oma::Rational edl_rate,
                                                      oma::SampleRate mixer_rate) {
    oma::ir::ImportReport report;
    auto doc = oma::ingest::import_timeline_file(path, importer, report, edl_rate);
    for (const auto& i : report.issues)
        issues << QString::fromStdString(i.object + ": " + i.reason + "; " + i.impact);
    if (!doc)
        why = QString::fromStdString(doc.error().summary());
    else
        note_silent_speeds(*doc, mixer_rate, issues);
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
