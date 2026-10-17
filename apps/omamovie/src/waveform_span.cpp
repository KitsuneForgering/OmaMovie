#include "waveform_span.hpp"

#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

BarSpan visible_bars(const QQuickItem& item, int bars, double step) {
    const QQuickWindow* window = item.window();
    if (item.width() <= kWideWaveform || window == nullptr || step <= 0.0) {
        return {.first = 0, .end = bars};
    }
    // The window's span in item coordinates, widened by a window either side so a short
    // scroll shows bars already built.
    const double left = item.mapToScene(QPointF(0, 0)).x();
    const auto screen = static_cast<double>(window->width());
    const double from = -left - screen;
    const double to = -left + (2.0 * screen);
    const auto index = [&](double bar) {
        return static_cast<int>(std::clamp(bar, 0.0, static_cast<double>(bars)));
    };
    const int first = index(std::floor(from / step));
    return {.first = first, .end = std::max(first, index(std::ceil(to / step)))};
}
