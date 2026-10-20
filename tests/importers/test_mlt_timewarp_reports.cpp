// What a timewarp import cannot play, reported (mlt_timewarp.hpp).
#include "mlt_timewarp.hpp"

#include <algorithm>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

// Kdenlive's pitch compensation is not played: the clip keeps its speed and says so.
bool reports_pitch_compensation() {
    const auto r = oma::importers::import_mlt(warped("2", "1"));
    return only_clip(r) && std::ranges::any_of(r->report.issues, [](const ir::ImportIssue& i) {
               return i.reason == "timewarp pitch compensation not imported";
           });
}

// A reverse or unreadable speed is a gap of the entry's length, with the reason.
bool leaves_gap(const char* speed) {
    const auto r = oma::importers::import_mlt(warped(speed));
    if (!r || r->timeline.tracks.size() != 1 || r->timeline.tracks[0].items.size() != 1)
        return false;
    const auto* gap = std::get_if<ir::Gap>(&r->timeline.tracks[0].items[0]);
    return gap && gap->duration.seconds_approx() * 30 == 4 &&
           std::ranges::any_of(r->report.issues, [](const ir::ImportIssue& i) {
               return i.reason == "reverse or unreadable timewarp speed not imported";
           });
}

// Kdenlive's time remap is a `timeremap` link inside a chain: the clip keeps its length at 1x.
bool reports_time_remap() {
    const auto r = oma::importers::import_mlt(
        R"(<mlt><profile frame_rate_num="30" frame_rate_den="1"/><chain id="c" out="29">)"
        R"(<property name="resource">/m/a.mp4</property><link id="l0">)"
        R"(<property name="mlt_service">timeremap</property></link></chain><playlist id="p">)"
        R"(<entry producer="c" in="0" out="9"/></playlist><tractor id="t">)"
        R"(<track producer="p"/></tractor></mlt>)");
    return only_clip(r) && r->report.issues.size() == 1 &&
           r->report.issues[0].reason == "time remap not imported";
}

} // namespace

void run_mlt_timewarp_report_tests() {
    describe("MLT timewarp import reports", {
        it("reports pitch compensation it does not play",
           { expect(reports_pitch_compensation()).toBeTruthy(); });
        it("reports a Kdenlive time remap it plays at normal speed",
           { expect(reports_time_remap()).toBeTruthy(); });
        it("leaves reverse and unreadable speeds as reported gaps", {
            expect(leaves_gap("-1")).toBeTruthy();
            expect(leaves_gap("0")).toBeTruthy();
            expect(leaves_gap("fast")).toBeTruthy();
            expect(leaves_gap("1234567")).toBeTruthy();
        });
    });
}
