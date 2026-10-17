#include "fixture.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

void run_media_color_tests() {
    describe("timeline edits: media color interpretation", {
        it("sets, undoes and redoes a media item's matrix and range", {
            auto fx = make_fixture();
            const auto read = [&] {
                return fx.editor.timeline().find_media(kCamera);
            };
            expect(read()->color_matrix == MediaColorMatrix::Auto).toBeTruthy();
            expect(fx.editor
                       .execute(edit::set_media_color(kCamera, MediaColorMatrix::Bt601,
                                                      MediaColorRange::Full))
                       .has_value())
                .toBeTruthy();
            expect(read()->color_matrix == MediaColorMatrix::Bt601 &&
                   read()->color_range == MediaColorRange::Full)
                .toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(read()->color_matrix == MediaColorMatrix::Auto &&
                   read()->color_range == MediaColorRange::Auto)
                .toBeTruthy();
            expect(fx.editor.redo().has_value()).toBeTruthy();
            expect(read()->color_matrix == MediaColorMatrix::Bt601).toBeTruthy();
        });

        it("refuses unknown media", {
            auto fx = make_fixture();
            expect(fx.editor
                       .execute(edit::set_media_color(MediaId{99}, MediaColorMatrix::Bt709,
                                                      MediaColorRange::Auto))
                       .has_value())
                .toBeFalsy();
        });
    });
}
