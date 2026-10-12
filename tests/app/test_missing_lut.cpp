#include "session.hpp"
#include "wait_for.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
bool refuses_export_with_missing_lut() {
    QTemporaryDir dir;
    const QString cube = dir.filePath("grade.cube");
    if (!QFile::copy("tests/fuzz/corpus/cube/identity-2.cube", cube))
        return false;
    Session original;
    original.setSilent(true);
    original.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return original.clips().size() == 1 && !original.importsPending(); }))
        return false;
    original.selectClip(original.clips().front().toMap().value("id").toDouble());
    original.importLut(QUrl::fromLocalFile(cube));
    if (!wait_for([&] {
            return original.luts().size() == 1 && original.info().value("lutAmount").toDouble() > 0;
        }))
        return false;
    const QString project = dir.filePath("graded.omamovie");
    original.saveProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return original.projectPath() == project && !original.dirty(); }))
        return false;
    if (!QFile::remove(cube))
        return false;
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] {
            return reopened.projectPath() == project && reopened.clips().size() == 1 &&
                   reopened.notice().contains("grade.cube");
        }))
        return false;
    const QString movie = dir.filePath("graded.mp4");
    reopened.exportMovie(QUrl::fromLocalFile(movie), QStringLiteral("mp4-h264"));
    return reopened.exportProgress() < 0 && reopened.notice().contains("grade.cube") &&
           !QFileInfo::exists(movie);
}
} // namespace

void run_missing_lut_tests() {
    describe("Export requirements", {
        it("refuses an export that would silently omit a missing LUT",
           { expect(refuses_export_with_missing_lut()).toBeTruthy(); });
    });
}
