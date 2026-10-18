#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
// Pointer events that keep the same snapped frame must not simulate the edit again, while a
// new frame or a changed timeline must.
bool repeats_only_changed_trims() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    const double id = session.clips().front().toMap().value("id").toDouble();
    int updates = 0;
    QObject::connect(&session, &Session::editScopeChanged, [&] { ++updates; });
    session.previewTrim(id, false, -5);
    session.previewTrim(id, false, -5);
    session.previewTrim(id, false, -5);
    const bool skipped = updates == 1 && session.editScope().value("active").toBool();
    session.previewTrim(id, false, -6);
    const bool new_frame = updates == 2;
    session.seek(0.5);
    session.splitAtPlayhead(); // the timeline changed under the same pointer position
    session.undo();
    const int before = updates;
    session.previewTrim(id, false, -6);
    const bool new_revision = updates == before + 1;
    session.clearEditScope();
    session.previewTrim(id, false, -6);
    const bool after_release = updates == before + 3; // the clear and the new preview
    return skipped && new_frame && new_revision && after_release;
}
} // namespace

void run_trim_preview_tests() {
    describe("Trim preview", {
        it("simulates a drag step only when its frame or the timeline changes",
           { expect(repeats_only_changed_trims()).toBeTruthy(); });
    });
}
