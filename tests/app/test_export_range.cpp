#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include "frame_match.hpp"
#include "loudness_match.hpp"

#include "oma_test.hpp"

namespace {
constexpr auto kSteps = "tests/fixtures/generated/h264_30fps_steps.mkv";

// Source frames [6, 16) marked in the source viewer and overwritten at sequence frame 3, then
// exported in software. Returns the movie, or empty.
QString export_marked_range(const QTemporaryDir& dir) {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo(kSteps).absoluteFilePath());
    if (!dir.isValid() ||
        !wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return {};
    session.openSource(0);
    session.seekSource(0.2);
    session.markIn();
    session.seekSource(0.5);
    session.markOut();
    session.closeSource();
    session.seek(0.1);
    session.overwriteSelected();
    if (session.clips().size() != 3)
        return {};
    const QString movie = dir.filePath("range.mp4");
    session.setExportEncoder("software");
    session.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return session.exportProgress() < 0; }, 60000);
    return done && session.exportedFile() == movie ? movie : QString();
}

// Exported frame -> the source frame its picture and its sound must come from: the range
// replaces frames 3..12 with source 6..15, and the rest of the clip keeps its own frames.
bool exports_the_marked_frames_and_samples() {
    QTemporaryDir dir;
    const QString movie = export_marked_range(dir);
    if (movie.isEmpty())
        return false;
    const QByteArray source = gray_frames(QFileInfo(kSteps).absoluteFilePath());
    const QByteArray out = gray_frames(movie);
    const auto samples = first_channel(movie);
    const auto shows = [&](int frame, int from) {
        return closest(source, out, frame) == from && heard_frame(samples, frame) == from;
    };
    return out.size() == 30 * kFrame && shows(2, 2) && shows(3, 6) && shows(12, 15) &&
           shows(13, 13) && shows(29, 29);
}
} // namespace

void run_export_range_tests() {
    describe("Exported source range", {
        it("shows and plays exactly the marked source frames in an independent decode",
           { expect(exports_the_marked_frames_and_samples()).toBeTruthy(); });
    });
}
