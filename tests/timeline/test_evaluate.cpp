#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

Composition at(const Fixture& fx, std::int64_t frame) {
    return evaluate(fx.editor.timeline(), f(frame)).value();
}

// Media time of the n-th video layer, in 30 fps frames of the 90 kHz camera timebase.
std::int64_t layer_frame(const Composition& c, std::size_t n) {
    return c.video.at(n).media_time.value() / 3000;
}

} // namespace

void run_evaluate_tests() {
    describe("timeline evaluation", {
        it("is empty where nothing plays", {
            auto fx = make_fixture();
            place(fx, fx.video, 10, 0, 10);
            const Composition c = at(fx, 5);
            expect(c.video.empty() && c.audio.empty()).toBeTruthy();
        });

        it("maps timeline time to media time through the source in", {
            auto fx = make_fixture();
            place(fx, fx.video, 10, 50, 20);
            const Composition c = at(fx, 15);
            expect(c.video.size()).toBe(1U);
            expect(layer_frame(c, 0)).toBe(55LL);
            expect(c.video[0].media_time.timebase() == q(1, 90000)).toBeTruthy();
        });

        it("stacks video tracks bottom first and skips hidden tracks", {
            auto fx = make_fixture();
            const ClipId base = place(fx, fx.video, 0, 0, 30);
            const ClipId top = place(fx, fx.overlay, 0, 100, 30);
            Composition c = at(fx, 3);
            expect(c.video.size()).toBe(2U);
            expect(c.video[0].clip == base && c.video[1].clip == top).toBeTruthy();
            expect(fx.editor.execute(edit::set_track_flags(fx.overlay, false, true)).has_value())
                .toBeTruthy();
            c = at(fx, 3);
            expect(c.video.size()).toBe(1U);
            expect(c.video[0].clip == base).toBeTruthy();
        });

        it("applies the clip speed", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 0, 40, 30);
            expect(fx.editor.execute(edit::set_speed(id, q(2, 1), false)).has_value()).toBeTruthy();
            expect(layer_frame(at(fx, 10), 0)).toBe(60LL);
            expect(at(fx, 10).audio.front().speed == q(2, 1)).toBeTruthy();
        });

        it("carries the clip's visual properties", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 0, 0, 30);
            VideoProperties v;
            v.opacity = 0.25F;
            v.crop.left = 0.1;
            expect(fx.editor.execute(edit::set_video(id, v)).has_value()).toBeTruthy();
            expect(at(fx, 0).video[0].video == v).toBeTruthy();
        });

        it("plays the audio of video clips and audio tracks unless muted", {
            auto fx = make_fixture();
            const ClipId cam = place(fx, fx.video, 0, 0, 30);
            const ClipId music = fx.editor.new_clip_id();
            expect(fx.editor
                       .execute(edit::overwrite(
                           fx.audio, music, f(0),
                           edit::ClipSource{.media = kMusic,
                                            .source_in =
                                                oma::RationalTime::make(0, q(1, 48000)).value(),
                                            .duration = f(30),
                                            .time_map = {},
                                            .video = {},
                                            .audio = {}}))
                       .has_value())
                .toBeTruthy();
            Composition c = at(fx, 15);
            expect(c.audio.size()).toBe(2U);
            const AudioSource& m = c.audio[1];
            expect(m.clip == music).toBeTruthy();
            expect(m.media_time.value()).toBe(24000LL); // 15 frames at 48 kHz
            expect(m.clip_offset == f(15) && m.clip_duration == f(30)).toBeTruthy();

            AudioProperties muted;
            muted.muted = true;
            expect(fx.editor.execute(edit::set_audio(cam, muted)).has_value()).toBeTruthy();
            expect(fx.editor.execute(edit::set_track_flags(fx.audio, true, false)).has_value())
                .toBeTruthy();
            c = at(fx, 15);
            expect(c.audio.empty()).toBeTruthy();
            expect(c.video.size()).toBe(1U);
        });

        it("shows a still image at any time", {
            auto fx = make_fixture();
            const ClipId id = fx.editor.new_clip_id();
            expect(fx.editor
                       .execute(edit::overwrite(fx.overlay, id, f(0),
                                                edit::ClipSource{.media = kPicture,
                                                                 .source_in = {},
                                                                 .duration = f(900),
                                                                 .time_map = {},
                                                                 .video = {},
                                                                 .audio = {}}))
                       .has_value())
                .toBeTruthy();
            const Composition c = at(fx, 899);
            expect(c.video.size() == 1U && c.audio.empty()).toBeTruthy();
        });

        it("rejects a time off the sequence grid", {
            auto fx = make_fixture();
            const auto half = oma::RationalTime::make(1, q(1, 60)).value();
            expect(evaluate(fx.editor.timeline(), half).has_value()).toBeFalsy();
        });
    });
}
