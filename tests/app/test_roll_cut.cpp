#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
double field(const QVariant& clip, const char* key) {
    return clip.toMap().value(key).toDouble();
}

// Alt+dragging the second clip's head three frames left rolls the cut with the first clip:
// previewed first, then one edit that keeps the sequence length.
bool rolls_a_cut_from_either_side() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.seek(0.5);
    session.splitAtPlayhead();
    const double length = session.duration();
    const double first = field(session.clips().at(0), "id");
    const double second = field(session.clips().at(1), "id");
    session.rollCut(first, true, -3); // nothing before the first clip: no edit
    const bool refused = std::abs(field(session.clips().at(0), "duration") - 0.5) < 1e-9;
    session.previewRoll(second, true, -3);
    const QVariantMap scope = session.editScope();
    const bool previewed =
        scope.value("cut").toList().contains(first) &&
        scope.value("moved").toMap().value(QString::number(second)).toInt() == -1;
    session.rollCut(second, true, -3);
    const QVariantList clips = session.clips();
    const bool rolled = std::abs(field(clips.at(0), "duration") - 12.0 / 30) < 1e-9 &&
                        std::abs(field(clips.at(1), "start") - 12.0 / 30) < 1e-9 &&
                        std::abs(session.duration() - length) < 1e-9 &&
                        !session.editScope().value("active").toBool();
    const bool shown = wait_for([&] { return session.edgeFrames().value("serial").toInt() == 1; });
    return refused && previewed && rolled && shown;
}
} // namespace

void run_roll_cut_tests() {
    describe("Roll", {
        it("previews and rolls a cut from the clip after it",
           { expect(rolls_a_cut_from_either_side()).toBeTruthy(); });
    });
}
