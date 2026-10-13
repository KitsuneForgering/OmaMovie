#include "scopes.hpp"

#include <algorithm>
#include <cmath>

QImage vectorscope(const QImage& picture) {
    const QImage rgb = picture.convertToFormat(QImage::Format_RGB32);
    QImage out(256, 256, QImage::Format_Grayscale8);
    out.fill(0);
    // A faint cross through neutral (Cb = Cr = 0) to read hue and saturation against.
    for (int i = 0; i < 256; ++i) {
        out.scanLine(128)[i] = 40;
        out.scanLine(i)[128] = 40;
    }
    for (int y = 0; y < rgb.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < rgb.width(); ++x) {
            const double r = qRed(line[x]) / 255.0;
            const double g = qGreen(line[x]) / 255.0;
            const double b = qBlue(line[x]) / 255.0;
            // BT.709 colour differences of the display-encoded values, each in [-0.5, 0.5].
            const double luma = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
            const double cb = (b - luma) / 1.8556;
            const double cr = (r - luma) / 1.5748;
            const int px = std::clamp(static_cast<int>(std::lround(128.0 + (cb * 254.0))), 0, 255);
            const int py = std::clamp(static_cast<int>(std::lround(128.0 - (cr * 254.0))), 0, 255);
            uchar& dot = out.scanLine(py)[px];
            dot = static_cast<uchar>(std::min(255, std::max<int>(dot, 96) + 24));
        }
    }
    return out;
}
