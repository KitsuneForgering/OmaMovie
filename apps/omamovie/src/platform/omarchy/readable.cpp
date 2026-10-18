#include "platform/omarchy/palette.hpp"

#include <QColor>

#include <cmath>

namespace omarchy {

namespace {

// WCAG 2.x relative luminance of an sRGB colour.
double luminance(const QColor& c) {
    const auto channel = [](double v) {
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return (0.2126 * channel(c.redF())) + (0.7152 * channel(c.greenF())) +
           (0.0722 * channel(c.blueF()));
}

QColor mix(const QColor& from, const QColor& to, double t) {
    return QColor::fromRgbF(static_cast<float>(from.redF() + ((to.redF() - from.redF()) * t)),
                            static_cast<float>(from.greenF() + ((to.greenF() - from.greenF()) * t)),
                            static_cast<float>(from.blueF() + ((to.blueF() - from.blueF()) * t)));
}

constexpr double kTextContrast = 4.5; // WCAG 1.4.3 AA for text at the UI's 11-13 px

} // namespace

double contrast_ratio(const QColor& a, const QColor& b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QVariantMap readable_palette(QVariantMap palette) {
    const QColor background(palette.value(QStringLiteral("background")).toString());
    const QColor foreground(palette.value(QStringLiteral("foreground")).toString());
    const QColor muted(palette.value(QStringLiteral("dark_foreground")).toString());
    if (!background.isValid() || !foreground.isValid() || !muted.isValid() ||
        contrast_ratio(muted, background) >= kTextContrast ||
        contrast_ratio(foreground, background) < kTextContrast) {
        return palette; // already readable, or the theme's own text cannot get there either
    }
    // The least mix toward the foreground that reaches the ratio, judged on the 8-bit colour the
    // UI will use: `hi` only ever takes mixes that pass, and t = 1 (the foreground) does.
    const auto stored = [&](double t) {
        return QColor(mix(muted, foreground, t).name());
    };
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < 24; ++i) {
        const double t = (lo + hi) / 2.0;
        (contrast_ratio(stored(t), background) >= kTextContrast ? hi : lo) = t;
    }
    palette.insert(QStringLiteral("dark_foreground"), stored(hi).name());
    return palette;
}

} // namespace omarchy
