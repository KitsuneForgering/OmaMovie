#include "decoded_frames.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
bool export_survives_project_switch() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    Session session;
    session.setSilent(true);
    session.setExportEncoder(QStringLiteral("software"));
    const QString input =
        QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath();
    session.open(input);
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.seek(0);
    const double cue = session.addCaption();
    if (cue <= 0)
        return false;
    session.setCaptionText(cue, QStringLiteral("Pinned caption"));
    const QString output = dir.filePath("movie.mp4");
    session.exportMovie(QUrl::fromLocalFile(output), QStringLiteral("mp4-h264"));
    session.newProject();
    if (!wait_for([&] { return session.exportProgress() < 0; }, 60000) ||
        session.exportedFile() != output || !session.clips().empty())
        return false;
    QFile captions(dir.filePath("movie.srt"));
    if (!captions.open(QIODevice::ReadOnly) || !captions.readAll().contains("Pinned caption"))
        return false;
    return decoded_frames(output) == 30;
}
} // namespace

void run_export_snapshot_tests() {
    describe("Export snapshot", {
        it("keeps the original movie and captions after a project switch",
           { expect(export_survives_project_switch()).toBeTruthy(); });
    });
}
