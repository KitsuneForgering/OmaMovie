#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
double field(const QVariantList& clips, int i, const char* key) {
    return clips.at(i).toMap().value(key).toDouble();
}

// Three clips of one file cut at 0.3 s and 0.6 s. Slipping the middle one changes only its
// source; sliding it moves its cuts and the neighbours absorb the frames.
bool slips_and_slides_the_middle_clip() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.seek(0.3);
    session.splitAtPlayhead();
    session.seek(0.6);
    session.splitAtPlayhead();
    const double middle = field(session.clips(), 1, "id");
    session.previewSlip(middle, true, 2);
    const QVariantMap scope = session.editScope();
    const bool previewed =
        scope.value("moved").toMap().value(QString::number(middle)).toInt() == 1 &&
        scope.value("cut").toList().size() == 1;
    session.slipClip(middle, false, -2);
    QVariantList c = session.clips();
    const bool slipped = std::abs(field(c, 1, "sourceIn") - 7.0 / 30) < 1e-9 &&
                         std::abs(field(c, 1, "start") - 0.3) < 1e-9;
    session.slipClip(middle, true, 2);
    c = session.clips();
    const bool slid = std::abs(field(c, 1, "start") - 11.0 / 30) < 1e-9 &&
                      std::abs(field(c, 0, "duration") - 11.0 / 30) < 1e-9 &&
                      std::abs(field(c, 2, "start") - 20.0 / 30) < 1e-9;
    const bool shown = wait_for([&] { return session.edgeFrames().value("serial").toInt() == 2; });
    return previewed && slipped && slid && shown;
}
} // namespace

void run_slip_slide_tests() {
    describe("Slip and slide", {
        it("slips the source or slides the clip with a previewed scope",
           { expect(slips_and_slides_the_middle_clip()).toBeTruthy(); });
    });
}
