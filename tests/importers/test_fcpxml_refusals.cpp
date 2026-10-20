// FCPXML documents the importer refuses, and ones it reads with a reported change.
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

// A retimed clip (Final Cut's 2x as a timeMap) is placed at its length and normal speed, and
// says so rather than play the wrong source silently.
bool reports_retiming() {
    const auto r = oma::importers::import_fcpxml(
        R"(<fcpxml version="1.10"><resources><format id="f" frameDuration="1/25s"/>)"
        R"(<asset id="a" start="0s" duration="10s" hasVideo="1" format="f">)"
        R"(<media-rep kind="original-media" src="file:///m/a.mp4"/></asset></resources>)"
        R"(<project name="p"><sequence format="f"><spine>)"
        R"(<asset-clip ref="a" offset="0s" name="fast" start="0s" duration="2s"><timeMap>)"
        R"(<timept time="0s" value="0s" interp="linear"/><timept time="2s" value="4s"/>)"
        R"(</timeMap></asset-clip></spine></sequence></project></fcpxml>)");
    return r && r->report.issues.size() == 1 &&
           r->report.issues[0].reason == "retiming (timeMap, conform-rate) not imported";
}

} // namespace

void run_fcpxml_refusal_tests() {
    describe("FCPXML refusals", {
        it("refuses what is not FCPXML", {
            expect(refused("<xmeml version=\"5\"/>")).toBeTruthy();
            expect(refused("<!DOCTYPE fcpxml><fcpxml/>")).toBeTruthy();
            expect(refused("not xml")).toBeTruthy();
        });
        it("reports a retimed clip it plays at normal speed",
           { expect(reports_retiming()).toBeTruthy(); });
        it("reads a sequence of unknown format at 25 fps and says so",
           { expect(reports_an_unknown_format()).toBeTruthy(); });
    });
}
