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

} // namespace

void run_mlt_timewarp_report_tests() {
    describe("MLT timewarp import reports", {
        it("reports pitch compensation it does not play",
           { expect(reports_pitch_compensation()).toBeTruthy(); });
        it("leaves reverse and unreadable speeds as reported gaps", {
            expect(leaves_gap("-1")).toBeTruthy();
            expect(leaves_gap("0")).toBeTruthy();
            expect(leaves_gap("fast")).toBeTruthy();
            expect(leaves_gap("1234567")).toBeTruthy();
        });
    });
}
