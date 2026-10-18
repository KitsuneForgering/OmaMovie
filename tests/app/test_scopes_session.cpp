#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QImage>
#include <QUrl>

#include "oma_test.hpp"

namespace {
// A 256 × 256 scope image with at least one lit pixel off the graticule.
bool drawn(const QString& url) {
    const QImage image(QUrl(url).toLocalFile());
    if (image.width() != 256 || image.height() != 256)
        return false;
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x)
            if (x != 128 && y != 128 && qGray(image.pixel(x, y)) >= 48)
                return true;
    return false;
}

// The session renders the frame at the playhead and publishes both scopes of it.
bool publishes_the_playhead_scopes() {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.requestScopes();
    if (!wait_for([&] { return !s.scopeImage().isEmpty() && !s.vectorscopeImage().isEmpty(); }))
        return false;
    return drawn(s.scopeImage()) && drawn(s.vectorscopeImage());
}
} // namespace

void run_scope_session_tests() {
    describe("Scopes in the session", {
        it("publishes the waveform and vectorscope of the frame at the playhead",
           { expect(publishes_the_playhead_scopes()).toBeTruthy(); });
    });
}
