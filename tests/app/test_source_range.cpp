#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
bool lasts(const QVariant& clip, double seconds) {
    return std::abs(clip.toMap().value("duration").toDouble() - seconds) < 1e-9 &&
           std::abs(clip.toMap().value("sourceIn").toDouble() - 0.2) < 1e-9;
}

// A [6, 16) frame range at 30 fps, overwritten onto the storyline and connected above it.
bool places_marked_range_by_overwrite_and_connect() {
    Session session;
    session.setSilent(true);
    session.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    const double length = session.duration();
    session.openSource(0);
    session.seekSource(0.2);
    session.markIn();
    session.seekSource(0.5);
    session.markOut();
    session.closeSource();
    session.seek(0.1);
    session.overwriteSelected();
    const QVariantList storyline = session.clips();
    const bool overwritten = storyline.size() == 3 && lasts(storyline.at(1), 10.0 / 30.0) &&
                             std::abs(session.duration() - length) < 1e-9;
    session.undo();
    session.connectSelected();
    const QVariantList lanes = session.videoTracks();
    const bool connected = session.clips().size() == 1 && lanes.size() == 1 &&
                           lasts(lanes.front().toMap().value("clips").toList().front(), 10.0 / 30);
    session.undo();
    return overwritten && connected && session.videoTracks().isEmpty();
}
} // namespace

void run_source_range_tests() {
    describe("Source range", {
        it("overwrites and connects exactly the marked range",
           { expect(places_marked_range_by_overwrite_and_connect()).toBeTruthy(); });
    });
}
