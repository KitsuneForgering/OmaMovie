#include "export_bundle_publish.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <array>

#include "oma_test.hpp"

namespace {
bool put(const QString& path, const char* bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == QByteArray(bytes).size();
}

QByteArray get(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool failure_restores_pair(PublishStep failed_step) {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const QString stage = dir.filePath("stage");
    if (!QDir().mkpath(stage))
        return false;
    const QString movie = dir.filePath("movie.mp4");
    const QString captions = dir.filePath("movie.srt");
    const QString next_movie = stage + "/movie.mp4";
    const QString next_captions = stage + "/movie.srt";
    if (!put(movie, "old movie") || !put(captions, "old captions") ||
        !put(next_movie, "new movie") || !put(next_captions, "new captions"))
        return false;
    const auto fault = [failed_step](PublishStep step) {
        return step == failed_step ? std::make_error_code(std::errc::io_error) : std::error_code{};
    };
    const auto result =
        publish_export_bundle(stage.toStdString(), movie.toStdString(), next_movie.toStdString(),
                              std::filesystem::path(captions.toStdString()),
                              std::filesystem::path(next_captions.toStdString()), fault);
    return !result && get(movie) == "old movie" && get(captions) == "old captions";
}

bool every_publication_failure_restores_pair() {
    for (auto step : {PublishStep::BackupMovie, PublishStep::BackupSidecar,
                      PublishStep::PublishMovie, PublishStep::PublishSidecar}) {
        if (!failure_restores_pair(step))
            return false;
    }
    return true;
}
} // namespace

void run_export_step_tests() {
    describe("Export publication steps", {
        it("restores the previous pair after failure at each rename",
           { expect(every_publication_failure_restores_pair()).toBeTruthy(); });
    });
}
