#include "scopes.hpp"

#include <QColor>

#include <algorithm>
#include <cmath>
#include <vector>

QImage luma_waveform(const QImage& picture) {
    const QImage rgb = picture.convertToFormat(QImage::Format_RGB32);
    const int w = rgb.width();
    const int h = rgb.height();
    std::vector<int> counts(static_cast<std::size_t>(w) * 256, 0);
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb p = line[x];
            const double luma = (0.2126 * qRed(p)) + (0.7152 * qGreen(p)) + (0.0722 * qBlue(p));
            const int level = std::clamp(static_cast<int>(std::lround(luma)), 0, 255);
            ++counts[(static_cast<std::size_t>(level) * static_cast<std::size_t>(w)) +
                     static_cast<std::size_t>(x)];
        }
    }
    // A column holds `h` pixels; scale so a level shared by a tenth of them is full white, and
    // keep any non-empty level visible.
    const double full = std::max(1.0, h / 10.0);
    QImage out(w, 256, QImage::Format_Grayscale8);
    for (int level = 0; level < 256; ++level) {
        uchar* row = out.scanLine(255 - level);
        for (int x = 0; x < w; ++x) {
            const int n = counts[(static_cast<std::size_t>(level) * static_cast<std::size_t>(w)) +
                                 static_cast<std::size_t>(x)];
            row[x] =
                n == 0 ? 0 : static_cast<uchar>(std::clamp(48.0 + (207.0 * n / full), 48.0, 255.0));
        }
    }
    return out;
}

QImage to_srgb_image(const oma::compositor::RgbaImage& image) {
    QImage out(static_cast<int>(image.width), static_cast<int>(image.height),
               QImage::Format_RGBA8888);
    const auto encode = [](float linear) {
        const double x = std::clamp(static_cast<double>(linear), 0.0, 1.0);
        const double v = x <= 0.0031308 ? 12.92 * x : (1.055 * std::pow(x, 1.0 / 2.4)) - 0.055;
        return static_cast<int>(std::lround(v * 255.0));
    };
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const auto p = image.at(x, y);
            const float a = p[3] > 0.0F ? p[3] : 1.0F;
            out.setPixelColor(
                static_cast<int>(x), static_cast<int>(y),
                QColor(encode(p[0] / a), encode(p[1] / a), encode(p[2] / a),
                       static_cast<int>(std::lround(std::clamp(p[3], 0.0F, 1.0F) * 255.0F))));
        }
    }
    return out;
}
