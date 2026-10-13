#include "frame_match.hpp"
#include "slip_export.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
// Split at frame 15, slip the second clip 3 frames earlier: the movie must show source frames
// 0..14 and then 12..26, checked by an independent decode of both files.
bool exports_the_slipped_frames() {
    QTemporaryDir dir;
    const QString media =
        QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath();
    const QString movie = export_slipped(dir, media);
    if (movie.isEmpty())
        return false;
    const QByteArray source = gray_frames(media);
    const QByteArray out = gray_frames(movie);
    return out.size() == 30 * kFrame && closest(source, out, 0) == 0 &&
           closest(source, out, 14) == 14 && closest(source, out, 15) == 12 &&
           closest(source, out, 29) == 26;
}
} // namespace

void run_export_frames_tests() {
    describe("Exported frames", {
        it("shows the slipped source frames in an independently decoded movie",
           { expect(exports_the_slipped_frames()).toBeTruthy(); });
    });
}
