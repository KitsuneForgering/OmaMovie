#include "scopes.hpp"

#include "oma_test.hpp"

namespace {
// The highest lit level in column x (0 bottom, 255 top), or -1 when empty.
int top_level(const QImage& scope, int x) {
    for (int row = 0; row < 256; ++row)
        if (qGray(scope.pixel(x, row)) > 0)
            return 255 - row;
    return -1;
}

// Pure red: its third of the parade traces at 255, the green and blue thirds at 0.
bool splits_channels_into_thirds() {
    QImage picture(96, 36, QImage::Format_RGB32);
    picture.fill(qRgb(255, 0, 0));
    const QImage p = rgb_parade(picture);
    return p.width() == 96 && p.height() == 256 && top_level(p, 10) == 255 &&
           top_level(p, 42) == 0 && top_level(p, 74) == 0;
}
} // namespace

void run_parade_tests() {
    describe("RGB parade", {
        it("traces each channel in its own third",
           { expect(splits_channels_into_thirds()).toBeTruthy(); });
    });
}
