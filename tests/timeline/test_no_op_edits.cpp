// Edits that change nothing leave no history entry (found by the edit properties, M10): an Undo
// that does nothing would cost the user a keypress and hide the edit before it.
#include "fixture.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

// Places one clip, clears the history, applies `edit` to it; true when the edit succeeded and
// left neither an undo entry nor a new revision.
template <typename Make>
bool leaves_no_trace(Make edit) {
    Fixture fx = make_fixture();
    const ClipId id = place(fx, fx.video, 10, 20, 30);
    fx.editor.clear_history();
    const auto revision = fx.editor.revision();
    const bool ok = fx.editor.execute(edit(fx.editor.timeline(), id)).has_value();
    return ok && !fx.editor.can_undo() && fx.editor.revision() == revision;
}

// A no-op after an undo keeps the redo it would otherwise discard.
bool keeps_redo() {
    Fixture fx = make_fixture();
    const ClipId id = place(fx, fx.video, 10, 20, 30);
    (void)fx.editor.execute(edit::trim_end(id, f(30), false));
    (void)fx.editor.undo();
    (void)fx.editor.execute(edit::disconnect(id));
    return fx.editor.can_redo();
}

// Connections Editor would drop while following them are refused rather than recorded.
bool refuses_same_track_connection() {
    Fixture fx = make_fixture();
    const ClipId a = place(fx, fx.video, 0, 0, 30);
    const ClipId b = place(fx, fx.video, 30, 0, 30);
    const ClipId over = place(fx, fx.overlay, 0, 0, 10);
    const ClipId top = place(fx, fx.overlay, 30, 0, 10);
    fx.editor.clear_history();
    const bool refused = !fx.editor.execute(edit::connect(a, a)) &&
                         !fx.editor.execute(edit::connect(b, a)) && !fx.editor.can_undo();
    // A chain is refused too: a primary that is connected, or a dependent other clips follow.
    const bool chained = fx.editor.execute(edit::connect(over, a)).has_value() &&
                         !fx.editor.execute(edit::connect(b, over)) &&
                         !fx.editor.execute(edit::connect(a, top));
    return refused && chained;
}

} // namespace

void run_no_op_edit_tests() {
    describe("Edits that change nothing", {
        it("disconnecting a clip that is not connected", {
            expect(leaves_no_trace([](const Timeline&, ClipId id) {
                return edit::disconnect(id);
            })).toBeTruthy();
        });
        it("trimming a start to where it is", {
            expect(leaves_no_trace([](const Timeline&, ClipId id) {
                return edit::trim_start(id, f(10), false);
            })).toBeTruthy();
        });
        it("sliding by zero", {
            expect(leaves_no_trace([](const Timeline&, ClipId id) {
                return edit::slide(id, f(0));
            })).toBeTruthy();
        });
        it("setting the speed a clip already has", {
            expect(leaves_no_trace([](const Timeline&, ClipId id) {
                return edit::set_speed(id, q(1, 1), true);
            })).toBeTruthy();
        });
        it("moving a clip to where it already is", {
            expect(leaves_no_trace([](const Timeline& tl, ClipId id) {
                return edit::move_clip(id, tl.track_of(id)->id, f(10));
            })).toBeTruthy();
        });
        it("refuses a connection on the clip's own track",
           { expect(refuses_same_track_connection()).toBeTruthy(); });
        it("keeps the redo stack", { expect(keeps_redo()).toBeTruthy(); });
    });
}
