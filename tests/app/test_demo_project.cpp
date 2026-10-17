#include "decoded_frames.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {

QByteArray center_pixel(const QString& movie, double seconds) {
    QProcess decoder;
    decoder.start("ffmpeg", {"-nostdin", "-v", "error", "-ss", QString::number(seconds), "-i",
                             movie, "-frames:v", "1", "-vf", "crop=2:2:320:180,scale=1:1", "-f",
                             "rawvideo", "-pix_fmt", "rgb24", "pipe:1"});
    if (!decoder.waitForFinished(30000) || decoder.exitCode() != 0)
        return {};
    return decoder.readAllStandardOutput();
}

bool opens_edits_and_exports_the_demo() {
    QTemporaryDir work;
    if (!work.isValid())
        return false;
    const QString bundled = QFileInfo("apps/omamovie/demo/First Edit.omamovie").absoluteFilePath();
    Session demo;
    demo.setSilent(true);
    demo.openDemoProject(QUrl::fromLocalFile(bundled));
    if (!wait_for([&] { return demo.clipCount() == 3 && !demo.importsPending(); }, 15000) ||
        demo.media().size() != 3 || !demo.projectPath().isEmpty() || demo.dirty() ||
        demo.projectName() != QStringLiteral("First edit demo"))
        return false;

    demo.selectMedia(2);
    demo.openSource(2);
    demo.seekSource(1.0);
    demo.markIn();
    demo.seekSource(1.5);
    demo.markOut();
    demo.appendSelected();
    if (demo.clipCount() != 4 || !demo.dirty())
        return false;

    const QString saved = work.filePath(QStringLiteral("my-first-edit.omamovie"));
    demo.saveProject(QUrl::fromLocalFile(saved));
    if (!wait_for([&] { return demo.projectPath() == saved && !demo.dirty(); }))
        return false;
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(saved));
    if (!wait_for([&] { return reopened.clipCount() == 4 && !reopened.importsPending(); }, 15000))
        return false;
    reopened.setExportEncoder(QStringLiteral("software"));
    const QString movie = work.filePath(QStringLiteral("first-edit.mp4"));
    reopened.exportMovie(QUrl::fromLocalFile(movie), QStringLiteral("mp4-h264"));
    if (!wait_for([&] { return reopened.exportProgress() < 0; }, 30000) ||
        reopened.exportedFile() != movie || decoded_frames(movie) <= 180)
        return false;

    // Decode the movie outside OmaMovie. Each original shot must still be in its intended
    // order after the source-range append and save/reopen.
    const QByteArray opening = center_pixel(movie, 0.5);
    const QByteArray detail = center_pixel(movie, 2.5);
    const QByteArray finale = center_pixel(movie, 4.5);
    if (opening.size() != 3 || detail.size() != 3 || finale.size() != 3)
        return false;
    const auto channel = [](const QByteArray& p, int i) {
        return static_cast<unsigned char>(p.at(i));
    };
    return channel(opening, 2) > channel(opening, 1) && channel(detail, 1) > channel(detail, 0) &&
           channel(finale, 0) > channel(finale, 1);
}

} // namespace

void run_demo_project_tests() {
    describe("Bundled demo edit", {
        it("opens as an unsaved copy and survives an edit, save, reopen and decoded export",
           { expect(opens_edits_and_exports_the_demo()).toBeTruthy(); });
    });
}
