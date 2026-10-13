// Stateful edit properties (M10): seeded command streams over the shared fixture, checked after
// every step and through a full undo/redo (Docs/implementation-plan-qa-timeline.md).
#include "fixture.hpp"
#include "property_invariants.hpp"
#include "property_run.hpp"

#include <string>

#include "oma_test.hpp"

namespace {

// Also counts useful work, so a generator whose steps are all rejected fails instead of passing
// vacuously. 48 seeds × 60 steps gave 2187 transitions and 296 of 441 step pairs (generator v3).
bool covers_enough(const property::Totals& totals) {
    return !totals.standard || (totals.transitions >= 1800 && totals.interactions.size() >= 220);
}

// The extent oracle flags a clip that shows media past the end of its source.
bool extent_oracle_can_fail() {
    auto fx = timeline_test::make_fixture();
    const auto id = timeline_test::place(fx, fx.video, 0, 0, 30);
    timeline_test::Clip clip = *fx.editor.timeline().find_clip(id);
    const bool fine = property::broken_extent(fx.editor.timeline(), clip).empty();
    clip.source_in = timeline_test::mf(290);
    return fine && !property::broken_extent(fx.editor.timeline(), clip).empty();
}

} // namespace

void run_property_tests() {
    describe("Edit properties", {
        it("hold for every seeded command stream", {
            const property::Totals totals = property::run_seeds();
            expect(totals.failures).toBe(std::string());
            expect(covers_enough(totals)).toBeTruthy();
        });
        it("replay a log exactly", {
            const property::Run run = property::run_seed(7, 60);
            const property::Run again = property::replay(run.log);
            expect(again.transitions == run.transitions && !again.failure).toBeTruthy();
        });
        it("have oracles that can fail", { expect(extent_oracle_can_fail()).toBeTruthy(); });
    });
}
