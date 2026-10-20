// FCPXML clip state the importer does not play as written: reported, never silently dropped.
#include "oma/importers/fcpxml.hpp"

#include <variant>

#include "oma_test.hpp"

namespace {

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

// A disabled spine clip keeps its place as a gap rather than play.
bool disabled_clip_is_a_gap() {
    const auto r = oma::importers::import_fcpxml(
        R"(<fcpxml version="1.10"><resources><format id="f" frameDuration="1/25s"/>)"
        R"(<asset id="a" start="0s" duration="10s" hasVideo="1" format="f">)"
        R"(<media-rep kind="original-media" src="file:///m/a.mp4"/></asset></resources>)"
        R"(<project name="p"><sequence format="f"><spine>)"
        R"(<asset-clip ref="a" offset="0s" name="off" start="0s" duration="2s" enabled="0"/>)"
        R"(</spine></sequence></project></fcpxml>)");
    return r && r->timeline.tracks.size() == 1 && r->timeline.tracks[0].items.size() == 1 &&
           std::holds_alternative<oma::ir::Gap>(r->timeline.tracks[0].items[0]) &&
           r->report.issues.size() == 1 &&
           r->report.issues[0].reason == "clip disabled in the source";
}

} // namespace

void run_fcpxml_report_tests() {
    describe("FCPXML clip reports", {
        it("reports a retimed clip it plays at normal speed",
           { expect(reports_retiming()).toBeTruthy(); });
        it("leaves a disabled clip as a gap", { expect(disabled_clip_is_a_gap()).toBeTruthy(); });
    });
}
