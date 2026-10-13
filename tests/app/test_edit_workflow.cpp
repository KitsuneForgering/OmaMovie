#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <cmath>

#include "oma_test.hpp"

namespace {
bool source_edit_round_trips_to_decoded_movie() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    Session session;
    session.setSilent(true);
    const QString media =
        QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath();
    session.open(media);
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    const double first = session.clips().front().toMap().value("id").toDouble();
    session.trimClip(first, false, -5);
    session.openSource(0);
    session.seekSource(0.2);
    session.markIn();
    session.seekSource(0.5);
    session.markOut();
    session.appendSelected();
    if (session.clips().size() != 2)
        return false;
    const double second = session.clips().at(1).toMap().value("id").toDouble();
    session.selectClip(first);
    session.nudgeRoll(-1);
    session.undo();
    session.selectClip(second);
    session.nudgeSlide(1);
    session.undo();
    session.nudgeSlip(1);
    const QString project = dir.filePath("edit.omamovie");
    session.saveProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return session.projectPath() == project && !session.dirty(); }))
        return false;
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return reopened.clips().size() == 2 && !reopened.importsPending(); }))
        return false;
    if (std::abs(reopened.clips().at(1).toMap().value("sourceIn").toDouble() - 7.0 / 30.0) > 1e-9)
        return false;
    const QString movie = dir.filePath("edit.mp4");
    reopened.setExportEncoder("software");
    reopened.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    if (!wait_for([&] { return reopened.exportProgress() < 0; }) ||
        reopened.exportedFile() != movie)
        return false;
    QProcess probe;
    probe.start("ffprobe",
                {"-v", "error", "-select_streams", "v:0", "-count_frames", "-show_entries",
                 "stream=nb_read_frames", "-of", "default=nw=1:nk=1", movie});
    return probe.waitForFinished(10000) && probe.exitCode() == 0 &&
           probe.readAllStandardOutput().trimmed() == "35";
}
} // namespace

void run_edit_workflow_tests() {
    describe("Edit workflow", {
        it("round-trips source marks and precise edits into a decoded movie",
           { expect(source_edit_round_trips_to_decoded_movie()).toBeTruthy(); });
    });
}
