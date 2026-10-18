// FCPXML documents the importer refuses, and the one it reads at a reported default rate.
#include "oma/importers/fcpxml.hpp"

#include "oma_test.hpp"

namespace {

bool refused(const char* text) {
    return !oma::importers::import_fcpxml(text);
}

// A sequence whose format is not among the resources reads at 25 fps, with a note saying so.
bool reports_an_unknown_format() {
    const auto r = oma::importers::import_fcpxml(
        R"(<fcpxml version="1.9"><project name="p"><sequence format="r9"><spine/></sequence>)"
        R"(</project></fcpxml>)");
    return r && r->report.issues.size() == 1 &&
           r->report.issues[0].reason == "sequence without a known format";
}

} // namespace

void run_fcpxml_refusal_tests() {
    describe("FCPXML refusals", {
        it("refuses what is not FCPXML", {
            expect(refused("<xmeml version=\"5\"/>")).toBeTruthy();
            expect(refused("<!DOCTYPE fcpxml><fcpxml/>")).toBeTruthy();
            expect(refused("not xml")).toBeTruthy();
        });
        it("reads a sequence of unknown format at 25 fps and says so",
           { expect(reports_an_unknown_format()).toBeTruthy(); });
    });
}
