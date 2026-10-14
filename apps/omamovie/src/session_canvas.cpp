// The canvas size (ADR-0010): one undo entry that also rescales every layer's position.
#include "session.hpp"

#include "oma/timeline/edit.hpp"

#include <algorithm>
#include <cmath>

namespace tl = oma::timeline;

void Session::setCanvasAspect(int w, int h) {
    if (!editor_ || w <= 0 || h <= 0) return;
    // Same short side, new proportion, even sizes: 1920x1080 becomes 1080x1920 at 9:16.
    const std::uint32_t short_side = std::min(canvas_width_, canvas_height_);
    const auto even = [](double v) { return static_cast<std::uint32_t>(std::lround(v / 2.0)) * 2U; };
    const std::uint32_t width = w >= h ? even(static_cast<double>(short_side) * w / h) : short_side;
    const std::uint32_t height = w >= h ? short_side : even(static_cast<double>(short_side) * h / w);
    if (width == canvas_width_ && height == canvas_height_) return;
    if (run(tl::edit::set_canvas(width, height))) {
        setNotice(QStringLiteral("Canvas %1 × %2").arg(width).arg(height));
    }
}
