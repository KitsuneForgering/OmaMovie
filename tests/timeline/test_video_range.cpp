#include "fixture.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {
VideoProperties left_half() {
    VideoProperties v;
    v.crop = {.left = 0.25, .top = 0.0, .right = 0.25, .bottom = 0.0};
    return v;
}

bool cropped(const Timeline& tl, TrackId track, std::size_t i) {
    return tl.find_track(track)->clips.at(i).video == left_half();
}

// Camera [0, 30) on the storyline.
Fixture one_clip(ClipId& clip) {
    Fixture fx = make_fixture();
    clip = place(fx, fx.video, 0, 0, 30);
    return fx;
}
} // namespace

void run_video_range_tests() {
    describe("timeline edits: video properties over a range", {
        it("splits at both edges and changes only the middle, in one undo entry", {
            ClipId clip;
            Fixture fx = one_clip(clip);
            expect(fx.editor.execute(edit::set_video_range(clip, f(10), f(20), left_half()))
                       .has_value())
                .toBeTruthy();
            const Timeline& tl = fx.editor.timeline();
            expect(layout(tl, fx.video)).toEqual("0+10@0 10+10@10 20+10@20");
            expect(tl.find_track(fx.video)->clips.at(0).id == clip).toBeTruthy();
            expect(cropped(tl, fx.video, 0) || cropped(tl, fx.video, 2)).toBeFalsy();
            expect(cropped(tl, fx.video, 1)).toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(layout(fx.editor.timeline(), fx.video)).toEqual("0+30@0");
            expect(cropped(fx.editor.timeline(), fx.video, 0)).toBeFalsy();
        });

        it("keeps the ID and splits once when the range starts at the clip", {
            ClipId clip;
            Fixture fx = one_clip(clip);
            expect(fx.editor.execute(edit::set_video_range(clip, f(0), f(12), left_half()))
                       .has_value())
                .toBeTruthy();
            const Timeline& tl = fx.editor.timeline();
            expect(layout(tl, fx.video)).toEqual("0+12@0 12+18@12");
            expect(tl.find_track(fx.video)->clips.at(0).id == clip).toBeTruthy();
            expect(cropped(tl, fx.video, 0)).toBeTruthy();
        });

        it("clamps a range wider than the clip and refuses one outside it", {
            ClipId clip;
            Fixture fx = one_clip(clip);
            expect(fx.editor.execute(edit::set_video_range(clip, f(20), f(90), left_half()))
                       .has_value())
                .toBeTruthy();
            expect(layout(fx.editor.timeline(), fx.video)).toEqual("0+20@0 20+10@20");
            expect(fx.editor.execute(edit::set_video_range(clip, f(25), f(28), left_half()))
                       .has_value())
                .toBeFalsy();
        });
    });
}
