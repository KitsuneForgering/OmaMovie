#include "title_raster.hpp"

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QString>

#include <algorithm>
#include <span>

namespace tl = oma::timeline;

oma::Result<oma::media::VideoFrame> rasterize_title(const tl::Title& title, std::uint32_t width,
                                                    std::uint32_t height) {
    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_Alpha8);
    if (image.isNull()) {
        return oma::make_error(oma::ErrorCode::Internal, oma::Category::Ui, "cannot allocate the title picture");
    }
    image.fill(0);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing);
        QFont font = title.font.empty() ? QGuiApplication::font() : QFont(QString::fromStdString(title.font));
        font.setPixelSize(std::max(1, static_cast<int>(title.size * height)));
        painter.setFont(font);
        painter.setPen(Qt::white); // coverage only: the colour is applied below
        const double w = width;
        const double h = height;
        // Safe margins of 6% of the width; the placement picks the band and its alignment.
        QRectF band(w * 0.06, h * 0.06, w * 0.88, h * 0.88);
        int align = Qt::AlignHCenter | Qt::TextWordWrap;
        switch (title.placement) {
        case tl::TitlePlacement::LowerThird:
            band = QRectF(w * 0.06, h * 0.55, w * 0.88, h * 0.35);
            align |= Qt::AlignBottom;
            break;
        case tl::TitlePlacement::Top:
            band = QRectF(w * 0.06, h * 0.08, w * 0.88, h * 0.35);
            align |= Qt::AlignTop;
            break;
        case tl::TitlePlacement::Center:
            align |= Qt::AlignVCenter;
            break;
        }
        painter.drawText(band, align, QString::fromStdString(title.text));
    }
    const auto byte = [](float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0F, 1.0F) * 255.0F + 0.5F); };
    const std::uint8_t r = byte(title.color[0]);
    const std::uint8_t g = byte(title.color[1]);
    const std::uint8_t b = byte(title.color[2]);
    const float opacity = std::clamp(title.color[3], 0.0F, 1.0F);
    const std::size_t row = static_cast<std::size_t>(width) * 4;
    std::vector<std::uint8_t> rgba(row * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        const uchar* coverage = image.constScanLine(static_cast<int>(y));
        std::uint8_t* out = rgba.data() + (y * row);
        for (std::uint32_t x = 0; x < width; ++x) {
            out[(x * 4) + 0] = r;
            out[(x * 4) + 1] = g;
            out[(x * 4) + 2] = b;
            out[(x * 4) + 3] = static_cast<std::uint8_t>(static_cast<float>(coverage[x]) * opacity + 0.5F);
        }
    }
    return oma::media::VideoFrame::from_rgba(static_cast<int>(width), static_cast<int>(height), rgba,
                                             static_cast<int>(row));
}
