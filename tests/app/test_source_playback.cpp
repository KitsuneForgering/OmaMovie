#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
bool source_plays_without_moving_sequence() {
    Session session;
    session.setSilent(true);
    const QString media =
        QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath();
    session.open(media);
    if (!wait_for([&] { return session.clips().size() == 1 && !session.importsPending(); }))
        return false;
    const double sequence = session.position();
    session.openSource(0);
    if (!session.source().value("open").toBool())
        return false;
    session.togglePlay();
    const bool advanced =
        wait_for([&] { return session.source().value("position").toDouble() >= 0.2; });
    const bool heard = session.takeAudioPeak() > 0.01F;
    session.pause();
    const bool independent = std::abs(session.position() - sequence) < 1e-9;
    session.closeSource();
    return advanced && heard && independent && !session.source().value("open").toBool();
}
} // namespace

void run_source_playback_tests() {
    describe("Source playback", {
        it("advances the source and mixes audio without moving the sequence",
           { expect(source_plays_without_moving_sequence()).toBeTruthy(); });
    });
}
