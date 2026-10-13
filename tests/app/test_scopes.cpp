#include "scopes.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QUrl>

#include "oma_test.hpp"

namespace {
// The level a column's brightest trace sits at (0 bottom, 255 top), or -1 when empty.
int trace_level(const QImage& waveform, int x) {
    for (int row = 0; row < 256; ++row)
        if (qGray(waveform.pixel(x, row)) > 0)
            return 255 - row;
    return -1;
}

// Left half black, right half white, and a pure green column (Y' = 0.7152 · 255 = 182).
bool traces_luma_per_column() {
    QImage picture(64, 36, QImage::Format_RGB32);
    picture.fill(Qt::black);
    for (int y = 0; y < 36; ++y)
        for (int x = 32; x < 64; ++x)
            picture.setPixel(x, y, qRgb(255, 255, 255));
    for (int y = 0; y < 36; ++y)
        picture.setPixel(10, y, qRgb(0, 255, 0));
    const QImage w = luma_waveform(picture);
    return w.width() == 64 && w.height() == 256 && trace_level(w, 0) == 0 &&
           trace_level(w, 63) == 255 && trace_level(w, 10) == 182 && qGray(w.pixel(0, 255)) == 255;
}
// The session renders the frame at the playhead and publishes its waveform.
bool publishes_the_playhead_waveform() {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.requestScopes();
    if (!wait_for([&] { return !s.scopeImage().isEmpty(); }))
        return false;
    const QImage w(QUrl(s.scopeImage()).toLocalFile());
    return w.width() == 256 && w.height() == 256 && trace_level(w, 128) > 0;
}
} // namespace

void run_scope_tests() {
    describe("Scopes", {
        it("traces each column's luma at its level",
           { expect(traces_luma_per_column()).toBeTruthy(); });
        it("publishes the waveform of the frame at the playhead",
           { expect(publishes_the_playhead_waveform()).toBeTruthy(); });
    });
}
