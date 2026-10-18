#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
// Opacity keys at 0.2 s and 0.6 s of a clip that starts at 0: one dopesheet row with both
// times; toggling at 0.2 s again removes that key from the row.
bool lists_keys_by_parameter() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    session.selectClip(session.clips().front().toMap().value("id").toDouble());
    const bool empty = session.keyTracks().isEmpty();
    for (const double at : {0.2, 0.6}) {
        session.seek(at);
        session.toggleOpacityKey();
    }
    const QVariantList rows = session.keyTracks();
    const QVariantList times =
        rows.size() == 1 ? rows.front().toMap().value("times").toList() : QVariantList{};
    const bool both = rows.size() == 1 && rows.front().toMap().value("name") == "Opacity" &&
                      times.size() == 2 && std::abs(times[0].toDouble() - 0.2) < 1e-6 &&
                      std::abs(times[1].toDouble() - 0.6) < 1e-6;
    session.seek(0.2);
    session.toggleOpacityKey();
    const QVariantList after = session.keyTracks();
    return empty && both && after.size() == 1 &&
           after.front().toMap().value("times").toList().size() == 1;
}
} // namespace

void run_key_tracks_tests() {
    describe("Dopesheet", {
        it("lists the selected clip's keys by parameter in sequence time",
           { expect(lists_keys_by_parameter()).toBeTruthy(); });
    });
}
