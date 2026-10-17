#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QTemporaryDir>

#include "oma_test.hpp"

namespace {
// Set on the selected clip's media, saved, reopened with it, and undone in one step.
bool keeps_the_interpretation_through_save() {
    QTemporaryDir dir;
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!dir.isValid() || !wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    s.setMediaColor(1, 2);
    const bool set =
        s.info().value("colorMatrix").toInt() == 1 && s.info().value("colorRange").toInt() == 2;
    const QString project = dir.filePath("color.omamovie");
    s.saveProject(QUrl::fromLocalFile(project));
    Session reopened;
    reopened.setSilent(true);
    if (!wait_for([&] { return !s.dirty(); }))
        return false;
    reopened.openProject(QUrl::fromLocalFile(project));
    if (!wait_for([&] { return reopened.clips().size() == 1 && !reopened.importsPending(); }))
        return false;
    reopened.selectClip(reopened.clips().front().toMap().value("id").toDouble());
    const bool kept = reopened.info().value("colorMatrix").toInt() == 1;
    s.undo();
    return set && kept && s.info().value("colorMatrix").toInt() == 0;
}
} // namespace

void run_color_session_tests() {
    describe("Color interpretation in the session", {
        it("keeps the interpretation through save and reopen",
           { expect(keeps_the_interpretation_through_save()).toBeTruthy(); });
    });
}
