#pragma once

class QQuickItem;

// Which of a waveform's bars to build. A clip item is as wide as the clip at the timeline's
// zoom, so a long clip zoomed in can be millions of pixels wide; building every bar then cost
// one vertex set per two pixels of the whole clip on each repaint (code review, 2026-10-14).
//
// Items up to kWideWaveform pixels build every bar and never depend on scrolling. Wider ones
// build the bars on screen plus one window width either side, and QML repaints them on scroll.
inline constexpr double kWideWaveform = 4096.0;

struct BarSpan {
    int first = 0; // first bar to build
    int end = 0;   // one past the last
};

// `bars` bars of `step` pixels across `item`; call from updatePaintNode (the UI thread waits).
[[nodiscard]] BarSpan visible_bars(const QQuickItem& item, int bars, double step);
