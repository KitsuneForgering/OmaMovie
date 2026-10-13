#include "scopes.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

QImage level_waveform(const QImage& picture, const std::function<int(QRgb)>& level_of) {
    const QImage rgb = picture.convertToFormat(QImage::Format_RGB32);
    const int w = rgb.width();
    const int h = rgb.height();
    const auto at = [w](int level, int x) {
        return (static_cast<std::size_t>(level) * static_cast<std::size_t>(w)) +
               static_cast<std::size_t>(x);
    };
    std::vector<int> counts(static_cast<std::size_t>(w) * 256, 0);
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < w; ++x)
            ++counts[at(std::clamp(level_of(line[x]), 0, 255), x)];
    }
    // A column holds `h` pixels; scale so a level shared by a tenth of them is full white, and
    // keep any non-empty level visible.
    const double full = std::max(1.0, h / 10.0);
    QImage out(w, 256, QImage::Format_Grayscale8);
    for (int level = 0; level < 256; ++level) {
        uchar* row = out.scanLine(255 - level);
        for (int x = 0; x < w; ++x) {
            const int n = counts[at(level, x)];
            row[x] =
                n == 0 ? 0 : static_cast<uchar>(std::clamp(48.0 + (207.0 * n / full), 48.0, 255.0));
        }
    }
    return out;
}

QImage luma_waveform(const QImage& picture) {
    return level_waveform(picture, [](QRgb p) {
        return static_cast<int>(
            std::lround((0.2126 * qRed(p)) + (0.7152 * qGreen(p)) + (0.0722 * qBlue(p))));
    });
}
