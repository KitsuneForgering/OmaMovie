#include "from_ir_sample.hpp"

#include <algorithm>

#include "oma_test.hpp"

namespace ir = oma::ir;
namespace tl = oma::timeline;
using namespace from_ir_sample;

void run_from_ir_tests() {
    describe("project::from_ir", {
        it("places clips, gaps and a dissolve on a 24 fps storyline and reports the rest", {
            ir::ImportReport report;
            auto doc = oma::project::from_ir(sample(), resolve, report);
            expect(doc.has_value() && doc->timeline.has_value()).toBeTruthy();
            const tl::Timeline& t = *doc->timeline;
            expect(t.frame_rate() == oma::frame_rates::k24).toBeTruthy();
            expect(doc->media.size()).toBe(1U);
            const tl::Track* v1 = t.find_track(doc->storyline);
            expect(v1 != nullptr && v1->clips.size() == 3).toBeTruthy();
            // a at 0 s, b after a and the gap at 2.5 s, c after the lost clip's gap at 4.5 s.
            expect(v1->clips[0].start.seconds_approx()).toEqual(0.0);
            expect(v1->clips[1].start.seconds_approx()).toEqual(2.5);
            expect(v1->clips[2].start.seconds_approx()).toEqual(4.5);
            expect(v1->clips[1].transition_in.has_value() &&
                   v1->clips[1].transition_in->duration.seconds_approx() == 0.5)
                .toBeTruthy();
            const auto mentions = [&](std::string_view what) {
                return std::ranges::any_of(
                    report.issues, [&](const ir::ImportIssue& i) { return i.object == what; });
            };
            expect(mentions("gone.mov") && mentions("lost")).toBeTruthy();
        });
    });
}
