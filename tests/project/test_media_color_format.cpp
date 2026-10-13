#include "oma/project/document.hpp"

#include <string>

#include "oma_test.hpp"

namespace tl = oma::timeline;

namespace {
oma::project::Document with_media(tl::MediaColorMatrix matrix, tl::MediaColorRange range) {
    oma::project::Document doc;
    oma::project::MediaRef m;
    m.info = {.id = tl::MediaId{1},
              .start = oma::RationalTime::make(0, oma::Rational::make(1, 30).value()).value(),
              .duration = oma::RationalTime::make(30, oma::Rational::make(1, 30).value()).value(),
              .has_video = true,
              .has_audio = false,
              .still = false,
              .color_matrix = matrix,
              .color_range = range};
    m.path = "/media/a.mov";
    m.name = "a.mov";
    doc.media.push_back(m);
    return doc;
}

std::string json_of(const oma::project::Document& doc) {
    return oma::project::to_json(doc, "/media").value_or(std::string());
}
} // namespace

void run_media_color_format_tests() {
    describe("project media color interpretation (format 7)", {
        it("round-trips an override and writes nothing for Auto", {
            const std::string text =
                json_of(with_media(tl::MediaColorMatrix::Bt601, tl::MediaColorRange::Full));
            auto back = oma::project::from_json(text, "/media");
            expect(back.has_value() && back->media.size() == 1).toBeTruthy();
            expect(back->media[0].info.color_matrix == tl::MediaColorMatrix::Bt601 &&
                   back->media[0].info.color_range == tl::MediaColorRange::Full)
                .toBeTruthy();
            expect(json_of(with_media(tl::MediaColorMatrix::Auto, tl::MediaColorRange::Auto))
                       .find("color_"))
                .toBe(std::string::npos);
        });

        it("refuses an unknown interpretation", {
            std::string text =
                json_of(with_media(tl::MediaColorMatrix::Bt709, tl::MediaColorRange::Auto));
            text.replace(text.find("\"bt709\""), 7, "\"bt999\"");
            expect(oma::project::from_json(text, "/media").has_value()).toBeFalsy();
        });
    });
}
