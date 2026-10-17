#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
bool near(double a, double b) {
    return std::abs(a - b) < 1e-6;
}

// Markers toggled at 0.6 s then 0.2 s come back in time order; next/previous jump between
// them; toggling at a marker removes it and undo puts it back.
bool toggles_and_jumps_between_markers() {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    for (const double at : {0.6, 0.2}) {
        s.seek(at);
        s.toggleMarker();
    }
    const QVariantList m = s.markers();
    const bool ordered = m.size() == 2 && near(m[0].toMap().value("time").toDouble(), 0.2) &&
                         near(m[1].toMap().value("time").toDouble(), 0.6);
    s.seek(0);
    s.seekMarker(1);
    const bool first = near(s.position(), 0.2);
    s.seekMarker(1);
    const bool second = near(s.position(), 0.6);
    s.seekMarker(-1);
    const bool back = near(s.position(), 0.2);
    s.toggleMarker();
    const bool removed = s.markers().size() == 1;
    s.undo();
    return ordered && first && second && back && removed && s.markers().size() == 2;
}
} // namespace

void run_marker_tests() {
    describe("Markers", {
        it("toggles markers at the playhead and jumps between them",
           { expect(toggles_and_jumps_between_markers()).toBeTruthy(); });
    });
}
