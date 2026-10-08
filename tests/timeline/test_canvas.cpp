#include "fixture.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

bool ok(Fixture& fx, std::unique_ptr<Command> c) {
    return fx.editor.execute(std::move(c)).has_value();
}

// A camera clip on the storyline, 192 px right and 108 px down, with one transform key.
ClipId offset_clip(Fixture& fx) {
    edit::ClipSource source = camera(0, 30);
    source.video.transform.offset_x = 192;
    source.video.transform.offset_y = 108;
    source.video.transform_keys = {TransformKey{
        .at = mf(0), .value = source.video.transform, .interpolation = Interpolation::Linear}};
    const ClipId id = fx.editor.new_clip_id();
    return ok(fx, edit::append(fx.video, id, source)) ? id : ClipId{};
}

const Transform& transform_of(const Fixture& fx, ClipId id) {
    return fx.editor.timeline().find_clip(id)->video.transform;
}

} // namespace

void run_canvas_tests() {
    describe("Canvas size (ADR-0010)", {
        it("is set without rescaling when it was unset", {
            auto fx = make_fixture();
            const ClipId id = offset_clip(fx);
            expect(ok(fx, edit::set_canvas(1920, 1080))).toBeTruthy();
            expect(fx.editor.timeline().canvas_width()).toBe(1920U);
            expect(transform_of(fx, id).offset_x).toBe(192.0);
        });

        it("rescales offsets and keys per axis, undone as one entry", {
            auto fx = make_fixture();
            const ClipId id = offset_clip(fx);
            expect(ok(fx, edit::set_canvas(1920, 1080))).toBeTruthy();
            expect(ok(fx, edit::set_canvas(1080, 1920))).toBeTruthy(); // 16:9 to 9:16
            expect(transform_of(fx, id).offset_x).toBeCloseTo(108.0, 1e-9);
            expect(transform_of(fx, id).offset_y).toBeCloseTo(192.0, 1e-9);
            expect(fx.editor.timeline().find_clip(id)->video.transform_keys[0].value.offset_y)
                .toBeCloseTo(192.0, 1e-9);
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.timeline().canvas_height()).toBe(1080U);
            expect(transform_of(fx, id).offset_x).toBe(192.0);
        });

        it("refuses odd, tiny and huge sizes", {
            auto fx = make_fixture();
            expect(ok(fx, edit::set_canvas(1921, 1080))).toBeFalsy();
            expect(ok(fx, edit::set_canvas(8, 8))).toBeFalsy();
            expect(ok(fx, edit::set_canvas(20000, 1080))).toBeFalsy();
            expect(fx.editor.timeline().canvas_width()).toBe(0U);
        });
    });
}
