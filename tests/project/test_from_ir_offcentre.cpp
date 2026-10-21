// Off-centre dissolves: OmaMovie centres every dissolve on its cut, so the import rolls the cut
// to the dissolve's middle and the same source frames mix as in the editor that wrote it.
#include "offcentre_sample.hpp"

#include "oma_test.hpp"

using namespace from_ir_sample;

namespace {

constexpr const char* kMoved = "cut moved to its middle; the same frames mix";

} // namespace

void run_from_ir_offcentre_tests() {
    describe("project::from_ir with an off-centre dissolve", {
        // melt: a's frames 48-59 mix into b's 100-111 before the cut at 60.
        it("rolls melt's mix (all before its cut) back to its middle", {
            const Cut c = imported(60, 12, 0, 112, 24);
            expect(c.a_frames == 54 && c.b_start == 54 && c.b_in == 106 && c.b_frames == 30 &&
                   c.dissolve == 12 && c.impact == kMoved)
                .toBeTruthy();
        });
        // CMX 3600: the dissolve starts at the cut, a's frames 48-59 into b's 100-111.
        it("rolls an EDL dissolve (all after its cut) forward to its middle", {
            const Cut c = imported(48, 0, 12, 100, 24);
            expect(c.a_frames == 54 && c.b_start == 54 && c.b_in == 106 && c.b_frames == 18 &&
                   c.dissolve == 12 && c.impact == kMoved)
                .toBeTruthy();
        });
        it("leaves a centred dissolve's cut and report alone", {
            const Cut c = imported(48, 6, 6, 100, 24);
            expect(c.a_frames == 48 && c.b_in == 100 && c.dissolve == 12 && c.impact.empty())
                .toBeTruthy();
        });
        // b starts at the media's first frame: nothing before it to roll the cut back over.
        it("centres it on the cut, and says so, without media to move the cut", {
            const Cut c = imported(60, 12, 0, 0, 24);
            expect(c.a_frames == 60 && c.b_in == 0 && c.dissolve > 0 &&
                   c.impact == "centred on the cut instead (no media to move it)")
                .toBeTruthy();
        });
    });
}
