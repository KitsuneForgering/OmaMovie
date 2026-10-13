#include "decoded_frames.hpp"
#include "file_bytes.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
// A 40-clip export still running when the user opens another file and edits it: the published
// pair describes only the sequence and captions as they were when the export started.
bool slow_export_survives_switch_and_edits() {
    QTemporaryDir dir;
    Session session;
    session.setSilent(true);
    session.setExportEncoder(QStringLiteral("software"));
    const QString clip =
        QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath();
    session.open(clip);
    if (!dir.isValid() ||
        !wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.selectMedia(0);
    for (int i = 1; i < 40; ++i)
        session.appendSelected();
    session.seek(0);
    session.setCaptionText(session.addCaption(), QStringLiteral("Pinned caption"));
    const QString movie = dir.filePath("movie.mp4");
    session.exportMovie(QUrl::fromLocalFile(movie), QStringLiteral("mp4-h264"));
    const bool running = wait_for([&] { return session.exportProgress() > 0.1; }, 30000) &&
                         session.exportProgress() < 0.9 && session.exportedFile() != movie;
    session.newProject();
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_steps.mkv").absoluteFilePath());
    wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); });
    session.setCaptionText(session.addCaption(), QStringLiteral("Later caption"));
    const bool unannounced = session.exportedFile() != movie;
    if (!wait_for([&] { return session.exportProgress() < 0; }, 120000) ||
        session.exportedFile() != movie)
        return false;
    const QByteArray captions = get(dir.filePath("movie.srt"));
    return running && unannounced && decoded_frames(movie) == 40 * 30 &&
           captions.contains("Pinned caption") && !captions.contains("Later caption");
}
} // namespace

void run_export_switch_tests() {
    describe("Export across a project switch", {
        it("publishes the snapshot taken at start while another project is edited",
           { expect(slow_export_survives_switch_and_edits()).toBeTruthy(); });
    });
}
