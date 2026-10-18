#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
bool has(const QVariantMap& scope, const char* key, double id) {
    return scope.value(key).toList().contains(id);
}

// Overwriting half of a one-clip sequence must show that clip as cut, without editing it.
bool previews_placements_without_editing() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    const double id = session.clips().front().toMap().value("id").toDouble();
    const bool undo_before = session.canUndo();
    session.selectMedia(0);
    session.seek(0.5);
    session.previewPlace(2);
    const QVariantMap overwrite = session.editScope();
    session.previewPlace(0);
    const QVariantMap append = session.editScope();
    session.clearEditScope();
    const bool unchanged = session.clips().size() == 1 && session.canUndo() == undo_before;
    return overwrite.value("active").toBool() && has(overwrite, "cut", id) &&
           !has(overwrite, "removed", id) && append.value("active").toBool() &&
           !has(append, "cut", id) && append.value("moved").toMap().isEmpty() && unchanged &&
           !session.editScope().value("active").toBool();
}
} // namespace

void run_place_preview_tests() {
    describe("Placement preview", {
        it("marks the clip an overwrite would cut and leaves the sequence alone",
           { expect(previews_placements_without_editing()).toBeTruthy(); });
    });
}
