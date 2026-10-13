#include "scopes.hpp"

#include <QColor>

#include <algorithm>
#include <cmath>

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
