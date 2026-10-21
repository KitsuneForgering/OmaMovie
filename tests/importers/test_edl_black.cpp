// CMX 3600 black (reel BL): a gap of its length, not a media file that cannot be found.
#include "oma/importers/edl.hpp"

#include <algorithm>
#include <variant>

#include "oma_test.hpp"

namespace ir = oma::ir;

namespace {

// Two seconds of black, a cut to A001, then a dissolve to black (25 fps).
constexpr auto kList =
    "TITLE: black\n"
    "001  BL       V     C        00:00:00:00 00:00:02:00 01:00:00:00 01:00:02:00\n"
    "002  A001     V     C        00:00:10:00 00:00:12:00 01:00:02:00 01:00:04:00\n"
    "003  A001     V     C        00:00:12:00 00:00:12:00 01:00:04:00 01:00:04:00\n"
    "003  BL       V     D    025 00:00:00:00 00:00:01:00 01:00:04:00 01:00:05:00\n";

bool black_is_a_gap() {
    const auto r = oma::importers::import_edl(kList, oma::Rational::literal(25, 1));
    if (!r || r->timeline.tracks.size() != 1 || r->timeline.media.size() != 1)
        return false;
    const auto& items = r->timeline.tracks[0].items;
    const auto* lead = items.size() == 3 ? std::get_if<ir::Gap>(&items[0]) : nullptr;
    const auto* tail = lead ? std::get_if<ir::Gap>(&items[2]) : nullptr;
    return tail && lead->duration.seconds_approx() == 2 && tail->duration.seconds_approx() == 1 &&
           std::holds_alternative<ir::Clip>(items[1]) &&
           std::ranges::any_of(r->report.issues, [](const ir::ImportIssue& i) {
               return i.reason == "transition D to black not imported";
           });
}

} // namespace

void run_edl_black_tests() {
    describe("EDL black", {
        it("reads black as a gap and reports a dissolve to black",
           { expect(black_is_a_gap()).toBeTruthy(); });
    });
}
