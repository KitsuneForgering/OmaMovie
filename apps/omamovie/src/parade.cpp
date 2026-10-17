#include "scopes.hpp"

#include <QPainter>

QImage rgb_parade(const QImage& picture) {
    // Each channel gets a third of the width, so the parade is as wide as the picture.
    const int third = std::max(1, picture.width() / 3);
    const QImage narrow =
        picture.scaled(third, picture.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage out(third * 3, 256, QImage::Format_Grayscale8);
    QPainter painter(&out);
    painter.drawImage(0, 0, level_waveform(narrow, [](QRgb p) { return qRed(p); }));
    painter.drawImage(third, 0, level_waveform(narrow, [](QRgb p) { return qGreen(p); }));
    painter.drawImage(2 * third, 0, level_waveform(narrow, [](QRgb p) { return qBlue(p); }));
    return out;
}
