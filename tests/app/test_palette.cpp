#include "platform/omarchy/palette.hpp"

#include <QColor>

#include "oma_test.hpp"

namespace {
QVariantMap theme(const char* background, const char* foreground, const char* muted) {
    return {{"background", background}, {"foreground", foreground}, {"dark_foreground", muted}};
}

double muted_contrast(const QVariantMap& p) {
    return omarchy::contrast_ratio(QColor(p.value("dark_foreground").toString()),
                                   QColor(p.value("background").toString()));
}
} // namespace

// Colours from the bundled Omarchy themes (omarchy-theme-color --all, 2026-10-14).
void run_palette_tests() {
    describe("Readable secondary text", {
        it("matches the WCAG contrast formula at its ends", {
            expect(omarchy::contrast_ratio(QColor("#000000"), QColor("#ffffff")))
                .toBeCloseTo(21.0, 1e-9);
            expect(omarchy::contrast_ratio(QColor("#777777"), QColor("#777777")))
                .toBeCloseTo(1.0, 1e-9);
        });
        it("raises dark and light themes to 4.5:1 with the least mix toward the foreground", {
            // everforest 1.72:1, white 1.82:1, rose-pine (light) 2.73:1
            for (const auto& p :
                 {theme("#2d353b", "#d3c6aa", "#4f585e"), theme("#ffffff", "#000000", "#c0c0c0"),
                  theme("#faf4ed", "#575279", "#9893a5")}) {
                const double raised = muted_contrast(omarchy::readable_palette(p));
                expect(raised >= 4.5 && raised < 4.6).toBeTruthy();
            }
        });
        it("keeps a theme that is already readable", {
            const auto osaka = theme("#111c18", "#C1C497", "#81B8A8"); // 7.78:1
            expect(omarchy::readable_palette(osaka) == osaka).toBeTruthy();
        });
    });
}
