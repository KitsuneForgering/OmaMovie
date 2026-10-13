#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <filesystem>

#include "export_bundle_publish.hpp"
#include "file_bytes.hpp"

#include "oma_test.hpp"

namespace {
bool restores_previous_pair_if_second_publish_fails() {
    QTemporaryDir dir;
    const QString stage = dir.filePath("stage");
    if (!QDir().mkpath(stage))
        return false;
    const QString movie = dir.filePath("movie.mp4");
    const QString captions = dir.filePath("movie.srt");
    const QString staged_movie = stage + "/movie.mp4";
    const QString missing_captions = stage + "/missing.srt";
    if (!put(movie, "old movie") || !put(captions, "old captions") ||
        !put(staged_movie, "new movie"))
        return false;
    const auto result =
        publish_export_bundle(stage.toStdString(), movie.toStdString(), staged_movie.toStdString(),
                              std::filesystem::path(captions.toStdString()),
                              std::filesystem::path(missing_captions.toStdString()));
    return !result && get(movie) == "old movie" && get(captions) == "old captions";
}
} // namespace

void run_export_bundle_tests() {
    describe("Export bundle", {
        it("restores the previous movie and captions when publishing fails",
           { expect(restores_previous_pair_if_second_publish_fails()).toBeTruthy(); });
    });
}
