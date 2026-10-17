#include "fixture.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {
// Storyline A [0, 15) and B [15, 30) from one camera take; B's sound detached onto the lane.
struct Detached {
    Fixture fx;
    ClipId a;
    ClipId b;
    ClipId sound;
};

Detached detached() {
    Detached d{make_fixture(), {}, {}, {}};
    d.a = place(d.fx, d.fx.video, 0, 0, 15);
    d.b = place(d.fx, d.fx.video, 15, 15, 15);
    d.sound = d.fx.editor.new_clip_id();
    (void)d.fx.editor.execute(edit::detach_audio(d.b, d.fx.audio, d.sound));
    return d;
}
} // namespace

void run_detach_tests() {
    describe("timeline edits: detached sound stays with its picture", {
        it("is connected to the picture at the same source frame", {
            auto d = detached();
            const Clip* s = d.fx.editor.timeline().find_clip(d.sound);
            expect(s != nullptr && s->anchor.has_value()).toBeTruthy();
            expect(s->anchor->primary == d.b && s->anchor->source == mf(15)).toBeTruthy();
        });

        it("moves with a slide of the picture and returns on undo", {
            auto d = detached();
            expect(d.fx.editor.execute(edit::slide(d.b, f(-2))).has_value()).toBeTruthy();
            expect(layout(d.fx.editor.timeline(), d.fx.audio)).toEqual("13+15@15");
            expect(d.fx.editor.undo().has_value()).toBeTruthy();
            expect(layout(d.fx.editor.timeline(), d.fx.audio)).toEqual("15+15@15");
        });

        it("moves with a ripple delete before it", {
            auto d = detached();
            expect(d.fx.editor.execute(edit::ripple_delete(d.a)).has_value()).toBeTruthy();
            expect(layout(d.fx.editor.timeline(), d.fx.audio)).toEqual("0+15@15");
        });
    });
}
