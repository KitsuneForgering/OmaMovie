#include "oma/base/error.hpp"

#include <string>

#include "oma_test.hpp"

using oma::Category;
using oma::Error;
using oma::ErrorCode;
using oma::Result;

namespace {

Result<int> parse_positive(int v) {
    if (v <= 0) {
        return oma::make_error(ErrorCode::InvalidArgument, Category::Project,
                               "value must be positive", "field 'duration'");
    }
    return v;
}

} // namespace

void run_error_tests() {
    describe("Error and Result", {
        it("carries code, category, message and context", {
            const Error e(ErrorCode::InvalidData, Category::Importer, "truncated header",
                          "clip.prproj");
            expect(static_cast<int>(e.code())).toEqual(static_cast<int>(ErrorCode::InvalidData));
            expect(static_cast<int>(e.category())).toEqual(static_cast<int>(Category::Importer));
            expect(e.message()).toEqual("truncated header");
            expect(e.context()).toEqual("clip.prproj");
        });

        it("describes itself for logs and UI warnings", {
            const Error e(ErrorCode::Overflow, Category::Timeline, "trim too long", "clip 4");
            expect(e.summary()).toEqual("[timeline] overflow: trim too long (clip 4)");
            const Error bare(ErrorCode::Cancelled, Category::Base, "stopped");
            expect(bare.summary()).toEqual("[base] cancelled: stopped");
        });

        it("propagates through Result", {
            expect(*parse_positive(3)).toEqual(3);
            auto r = parse_positive(-1);
            expect(r.has_value()).toBeFalsy();
            expect(r.error().context()).toEqual("field 'duration'");
        });
    });
}
