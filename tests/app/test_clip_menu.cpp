// Session::clipMenuTarget: what the keyboard context menu opens on (QML only places it).
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
QString kind(const Session& s) {
    return s.clipMenuTarget().value("kind").toString();
}

// The selected clip where it sits, else the cut at the playhead (to half a frame), else nothing.
bool targets_follow_selection_and_playhead() {
    Session s;
    s.setSilent(true);
    if (kind(s) != "empty")
        return false;
    s.open(QFileInfo("tests/fixtures/generated/h264_long_gop.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.seek(4.0);
    s.splitAtPlayhead();
    const QVariantList clips = s.clips();
    const double second = clips.at(1).toMap().value("id").toDouble();
    s.selectClip(0);
    const QVariantMap cut = s.clipMenuTarget();
    bool ok = cut.value("kind") == "cut" && cut.value("clipId").toDouble() == second &&
              cut.value("transitionKind").toInt() == -1;
    s.seek(4.0 + 1.0 / s.frameRate()); // one frame past the cut
    ok = ok && kind(s) == "empty";
    s.seek(0.0); // the first clip's start joins nothing
    ok = ok && kind(s) == "empty";
    s.selectClip(second);
    const QVariantMap selected = s.clipMenuTarget();
    ok = ok && selected.value("kind") == "storyline" && selected.value("start").toDouble() > 3.9;
    s.addTitle();
    const QVariantMap title = s.clipMenuTarget();
    return ok && title.value("kind") == "video" && title.value("lane").toInt() == 0 &&
           title.value("title").toBool() && s.videoLaneCount() == 1 && s.audioLaneCount() == 0;
}
} // namespace

void run_clip_menu_tests() {
    describe("Clip menu target", {
        it("follows the selection, then the cut at the playhead",
           { expect(targets_follow_selection_and_playhead()).toBeTruthy(); });
    });
}
