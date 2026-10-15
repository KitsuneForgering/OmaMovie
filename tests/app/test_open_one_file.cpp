// Opening one video (the launcher and --export-audit path) puts exactly one clip on the
// storyline and none on other lanes. An --export-audit run on 2026-10-14 composited two video
// layers over one frame (code review part 4); this pins what one file should produce.
#include "session.hpp"
#include "wait_for.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

#include "oma_test.hpp"

namespace {

int lane_clips(const QVariantList& lanes) {
    int n = 0;
    for (const QVariant& lane : lanes)
        n += static_cast<int>(lane.toMap().value(QStringLiteral("clips")).toList().size());
    return n;
}

struct Opened {
    int storyline = -1;
    int video_lanes = -1;
};

Opened open_one(const char* fixture) {
    Session s;
    s.setSilent(true);
    s.openFiles({QFileInfo(fixture).absoluteFilePath()});
    Opened out;
    if (!wait_for([&] { return !s.clips().isEmpty() && !s.importsPending(); }))
        return out;
    // Then export as --export-audit does: retried until the library is ready.
    QTemporaryDir dir;
    const QString movie = dir.filePath("out.mp4");
    for (int i = 0; i < 100 && s.exportProgress() < 0 && s.exportedFile() != movie; ++i) {
        s.exportMovie(QUrl::fromLocalFile(movie));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    (void)wait_for([&] { return s.exportProgress() < 0; });
    out.storyline = static_cast<int>(s.clips().size());
    out.video_lanes = lane_clips(s.videoTracks());
    return out;
}

} // namespace

void run_open_one_file_tests() {
    describe("Opening one file", {
        it("places one storyline clip and nothing else", {
            const Opened o = open_one("tests/fixtures/generated/h264_30fps_aac.mp4");
            expect(o.storyline).toBe(1);
            expect(o.video_lanes).toBe(0);
        });
    });
}
