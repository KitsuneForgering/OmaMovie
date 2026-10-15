#include "otio_test.hpp"

#include "oma/importers/edl.hpp"

#include "oma_test.hpp"

namespace {
bool refused(const char* text) {
    return !oma::importers::import_edl(text, oma::Rational::make(25, 1).value());
}
bool reported(const char* text) {
    const auto r = oma::importers::import_edl(text, oma::Rational::make(25, 1).value());
    return r && !r->report.issues.empty();
}
} // namespace

void run_edl_refusal_tests() {
    describe("EDL import refusals", {
        it("refuses malformed events and impossible timecodes", {
            expect(refused("001 AX V C 00:00:00:00 00:00:01:00 00:00:00:00\n")).toBeTruthy();
            expect(refused("001 AX V C 00:00:00:25 00:00:01:00 00:00:00:00 00:00:01:00\n"))
                .toBeTruthy();
            expect(refused("001 AX V C 00:00:02:00 00:00:01:00 00:00:00:00 00:00:01:00\n"))
                .toBeTruthy();
            // A line that is not an event before the first one, or an overlap, is reported.
            expect(reported("x01 AX V C 00:00:00:00 00:00:01:00 00:00:00:00 00:00:01:00\n"))
                .toBeTruthy();
            expect(reported("001 AX V C 00:00:00:00 00:00:01:00 00:00:00:00 00:00:01:00\n"
                            "002 AX V C 00:00:01:00 00:00:02:00 00:00:00:00 00:00:01:00\n"))
                .toBeTruthy();
        });
    });
}
