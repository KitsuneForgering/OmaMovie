// The bench's three-layer scene (tools/bench/cross-editor/layers.py) at fixture scale: the camera
// on V1 and connected above itself at 0.2 s and 0.4 s, V2 and V3 halved into the top-left and
// bottom-right quadrants, saved, reopened, exported and checked by an independent decode.
#include "quadrants.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
constexpr auto kCamera = "tests/fixtures/generated/h264_30fps_aac.mp4";

QString exported(Session& s, const QTemporaryDir& dir, const char* name) {
    const QString movie = dir.filePath(name);
    s.setExportEncoder("software");
    s.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return s.exportProgress() < 0; }, 60000);
    return done && s.exportedFile() == movie ? movie : QString();
}

// The camera alone (`layers` false), or the three-layer scene through save and reopen.
QString export_scene(const QTemporaryDir& dir, bool layers) {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo(kCamera).absoluteFilePath());
    if (!dir.isValid() || !wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return {};
    if (!layers)
        return exported(s, dir, "plain.mp4");
    s.selectMedia(0);
    for (const double at : {0.2, 0.4}) {
        s.seek(at);
        s.connectSelected();
        s.setClipTransform(at < 0.3 ? -80 : 80, at < 0.3 ? -45 : 45, 0.5, 0);
    }
    const QString project = dir.filePath("layers.omamovie");
    s.saveProject(QUrl::fromLocalFile(project));
    if (s.videoTracks().size() != 2 || !wait_for([&] { return !s.dirty(); }))
        return {};
    Session reopened;
    reopened.setSilent(true);
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return reopened.videoTracks().size() == 2 && !reopened.importsPending(); }))
        return {};
    return exported(reopened, dir, "layers.mp4");
}

// Output frame 20 against the camera exported alone: V2 shows source 14 halved in the top-left,
// V3 source 8 halved in the bottom-right, V1 frame 20 in the other two. A correct quadrant read
// about 2.3 on the reference host; the other layer's frame in it, about 11.
bool composites_three_layers() {
    QTemporaryDir dir;
    const QString plain = export_scene(dir, false);
    const QString movie = export_scene(dir, true);
    const QByteArray out = movie.isEmpty() ? QByteArray() : gray_at(movie, 20, false);
    const QByteArray v1 = gray_at(plain, 20, false);
    const QByteArray v2 = gray_at(plain, 14, true);
    const QByteArray v3 = gray_at(plain, 8, true);
    if (out.isEmpty() || v1.isEmpty() || v2.isEmpty() || v3.isEmpty())
        return false;
    constexpr double kMatch = 5.0;
    return quadrant_diff(out, 0, 0, v2) < kMatch && quadrant_diff(out, 1, 1, v3) < kMatch &&
           quadrant_diff(out, 1, 0, v1) < kMatch && quadrant_diff(out, 0, 1, v1) < kMatch &&
           quadrant_diff(out, 0, 0, v3) > 2 * kMatch && quadrant_diff(out, 1, 1, v2) > 2 * kMatch;
}
} // namespace

void run_shared_layers_tests() {
    describe("Shared cross-editor layers", {
        it("composites two halved layers over the storyline after save and reopen",
           { expect(composites_three_layers()).toBeTruthy(); });
    });
}
