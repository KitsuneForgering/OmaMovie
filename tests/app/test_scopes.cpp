#include "scopes.hpp"

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
// Grey sits at the centre; pure red reaches Cr = +0.5 (the top row), pure blue Cb = +0.5 (the
// right column); positions computed independently from the BT.709 equations.
bool places_chroma_on_the_plane() {
    QImage picture(3, 1, QImage::Format_RGB32);
    picture.setPixel(0, 0, qRgb(128, 128, 128));
    picture.setPixel(1, 0, qRgb(255, 0, 0));
    picture.setPixel(2, 0, qRgb(0, 0, 255));
    const QImage v = vectorscope(picture);
    const auto lit = [&](int x, int y) {
        return qGray(v.pixel(x, y)) >= 96;
    };
    return lit(128, 128) && lit(99, 1) && lit(255, 140);
}

} // namespace

void run_scope_tests() {
    describe("Scopes", {
        it("traces each column's luma at its level",
           { expect(traces_luma_per_column()).toBeTruthy(); });
        it("places chroma on the vectorscope plane",
           { expect(places_chroma_on_the_plane()).toBeTruthy(); });
    });
}
