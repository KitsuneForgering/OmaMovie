#include "color_fixture.hpp"
#include "color_interpretation.hpp"

#include "oma_test.hpp"

namespace tl = oma::timeline;

void run_color_interpretation_tests() {
    describe("Color interpretation", {
        it("reaches the picture the compositor gets", {
            const auto as_is =
                composited_color(tl::MediaColorMatrix::Auto, tl::MediaColorRange::Auto);
            const auto read =
                composited_color(tl::MediaColorMatrix::Bt601, tl::MediaColorRange::Full);
            expect(as_is.has_value() && read.has_value()).toBeTruthy();
            expect(read->matrix == 6 && read->range == oma::media::ColorRange::Full).toBeTruthy();
            expect(as_is->matrix != 6 || as_is->range != oma::media::ColorRange::Full).toBeTruthy();
        });

        it("leaves RGB pictures alone", {
            oma::media::ColorInfo rgb;
            rgb.matrix = 0;
            interpret(rgb, tl::MediaInfo{.id = {},
                                         .start = {},
                                         .duration = {},
                                         .has_video = true,
                                         .has_audio = false,
                                         .still = true,
                                         .color_matrix = tl::MediaColorMatrix::Bt601,
                                         .color_range = tl::MediaColorRange::Full});
            expect(rgb.matrix == 0 && rgb.range == oma::media::ColorRange::Unspecified)
                .toBeTruthy();
        });
    });
}
