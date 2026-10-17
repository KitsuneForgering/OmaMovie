#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
double motion(Session& s, const char* key) {
    return s.motion().value(key).toDouble();
}

// The fixture connected above itself on a 320x180 canvas: picture-in-picture is a third of the
// frame toward the lower right and undoes in one step; split screen moves it a quarter frame
// right and the storyline under it a quarter frame left, in one step; a storyline clip is refused.
bool lays_out_connected_clips() {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    const double storyline = s.clips().front().toMap().value("id").toDouble();
    s.selectMedia(0);
    s.seek(0.2);
    s.connectSelected();
    const double lane = s.selectedClip();
    if (s.videoTracks().size() != 1 || lane == storyline)
        return false;
    s.applyLayout(0);
    const bool pip = std::abs(motion(s, "scale") - 1.0 / 3.0) < 1e-9 && motion(s, "posX") > 0 &&
                     motion(s, "posY") > 0;
    s.undo();
    const bool undone = std::abs(motion(s, "scale") - 1.0) < 1e-9 && motion(s, "posX") == 0.0;
    s.applyLayout(1);
    const bool split = std::abs(motion(s, "posX") - 80.0) < 1e-9 && motion(s, "scale") == 1.0;
    // The storyline under it, cut where the overlay starts: the left half from there on only.
    const QVariantList cut = s.clips();
    const bool framed = cut.size() == 2 && s.undoText() == QStringLiteral("Split Screen") &&
                        std::abs(cut.at(1).toMap().value("start").toDouble() - 0.2) < 1e-9;
    s.selectClip(cut.value(1).toMap().value("id").toDouble());
    const bool left = std::abs(motion(s, "posX") + 80.0) < 1e-9;
    s.selectClip(storyline);
    const bool before = motion(s, "posX") == 0.0;
    s.undo();
    const bool whole = s.clips().size() == 1 && motion(s, "posX") == 0.0;
    s.redo();
    s.selectClip(storyline);
    s.applyLayout(0);
    const bool refused = motion(s, "scale") == 1.0 && motion(s, "posX") == 0.0;
    return pip && undone && split && framed && left && before && whole && refused;
}
} // namespace

void run_layout_tests() {
    describe("Intent layouts", {
        it("lays out a connected clip as picture-in-picture or split screen",
           { expect(lays_out_connected_clips()).toBeTruthy(); });
    });
}
