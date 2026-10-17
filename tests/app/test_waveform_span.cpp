// A long clip at full zoom is millions of pixels wide; its waveform builds only the bars near
// the screen (code review 2026-10-14), a narrow one builds them all.
#include "waveform_span.hpp"

#include <QQuickItem>
#include <QQuickWindow>

#include "oma_test.hpp"

namespace {

struct Span {
    int first = -1;
    int end = -1;
};

// An item `width` px wide whose left edge sits at `x` in a 1,000 px window; 2 px per bar.
Span span_for(double width, double x) {
    QQuickWindow window;
    window.resize(1000, 200);
    QQuickItem item(window.contentItem());
    item.setX(x);
    item.setSize(QSizeF(width, 20));
    const int bars = static_cast<int>(width / 2.0);
    const BarSpan s = visible_bars(item, bars, 2.0);
    return {.first = s.first, .end = s.end};
}

} // namespace

void run_waveform_span_tests() {
    describe("Waveform bars", {
        it("builds every bar of a clip narrower than the limit", {
            const Span s = span_for(3000, -500);
            expect(s.first).toBe(0);
            expect(s.end).toBe(1500);
        });
        it("builds only the bars near the screen of a very wide clip", {
            // A one-hour clip at 2,400 px/s, scrolled so x = 1,000,000 px is at the left edge.
            const Span s = span_for(8'640'000, -1'000'000);
            expect(s.first).toBe((1'000'000 - 1000) / 2); // a window before the screen
            expect(s.end).toBe((1'000'000 + 2000) / 2);   // and one after it
        });
        it("builds nothing for a wide clip far off screen", {
            const Span s = span_for(8'640'000, 50'000);
            expect(s.end - s.first).toBe(0);
        });
    });
}
