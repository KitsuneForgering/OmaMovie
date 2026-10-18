#include "export_bundle.hpp"
#include "export_bundle_publish.hpp"
#include "export_recovery.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
bool successful_publication_discards_old_pair() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath("stage");
    if (!QDir().mkpath(stage))
        return false;
    const QString movie = dir.filePath("movie.mp4");
    const QString sidecar = dir.filePath("movie.srt");
    const QString next_movie = stage + "/movie.mp4";
    const QString next_sidecar = stage + "/movie.srt";
    for (const auto& [path, bytes] : {std::pair{movie, "old movie"},
                                      {sidecar, "old captions"},
                                      {next_movie, "new movie"},
                                      {next_sidecar, "new captions"}}) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) < 0)
            return false;
    }
    const auto published =
        publish_export_bundle(stage.toStdString(), movie.toStdString(), next_movie.toStdString(),
                              std::filesystem::path(sidecar.toStdString()),
                              std::filesystem::path(next_sidecar.toStdString()));
    QFile result(movie);
    QFile captions(sidecar);
    return published && !QDir(stage).exists() && result.open(QIODevice::ReadOnly) &&
           captions.open(QIODevice::ReadOnly) && result.readAll() == "new movie" &&
           captions.readAll() == "new captions";
}

bool completed_bundle_forgets_its_recovery_record() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    auto timeline = oma::timeline::Timeline::create(oma::frame_rates::k30,
                                                    oma::SampleRate::make(48000).value());
    if (!timeline)
        return false;
    const QString file = dir.filePath("complete.mp4");
    ExportRequest request{.timeline = *timeline,
                          .paths = {},
                          .luts = {},
                          .file = file.toStdString(),
                          .width = 32,
                          .height = 32,
                          .frames = 1,
                          .ticks_per_frame = 1600,
                          .audio_rate = oma::SampleRate::make(48000).value(),
                          .encoder = ExportRequest::Encoder::Software};
    const auto result = export_movie_bundle(
        std::move(request), std::nullopt, [](std::int64_t, std::int64_t) { return true; }, nullptr);
    return result && QFile::exists(file) && export_recovery::pending().empty() &&
           QDir(dir.path()).entryList({".omamovie-export-*"}, QDir::Dirs | QDir::Hidden).empty();
}
} // namespace

void run_export_success_tests() {
    describe("Export publication completion", {
        it("publishes both files and removes the previous pair",
           { expect(successful_publication_discards_old_pair()).toBeTruthy(); });
        it("removes the recovery record after a normal export",
           { expect(completed_bundle_forgets_its_recovery_record()).toBeTruthy(); });
    });
}
