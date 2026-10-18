#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include <cmath>

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

// Clip A shows media frames 0-29 at 0, clip B frames 100-129 at 30, and B gets a transition of
// `frames` (centered on the cut at 30).
Fixture with_transition(TransitionKind kind, std::int64_t frames, std::int64_t b_in = 100) {
    auto fx = make_fixture();
    place(fx, fx.video, 0, 0, 30);
    const ClipId b = place(fx, fx.video, 30, b_in, 30);
    (void)fx.editor.execute(
        edit::set_transition(b, Transition{.kind = kind, .duration = f(frames)}));
    return fx;
}

bool near(double a, double b) {
    return std::abs(a - b) < 1e-6;
}

// A transform key at camera frame `frame`, moved `x` pixels right and scaled by `scale`.
TransformKey key(std::int64_t frame, double x, double scale,
                 Interpolation interpolation = Interpolation::Linear) {
    Transform t;
    t.offset_x = x;
    t.scale_x = scale;
    t.scale_y = scale;
    return TransformKey{.at = mf(frame), .value = t, .interpolation = interpolation};
}

// Clip from camera frame 0 at timeline 0, 60 frames, keyed from x 0 at frame 0 to 100 at 40.
Fixture keyed(Interpolation interpolation, ClipId* id) {
    auto fx = make_fixture();
    *id = place(fx, fx.video, 0, 0, 60);
    VideoProperties v;
    v.transform_keys = {key(0, 0, 1, interpolation), key(40, 100, 4)};
    (void)fx.editor.execute(edit::set_video(*id, v));
    return fx;
}

double x_at(const Fixture& fx, std::int64_t frame) {
    return at(fx, frame).video.at(0).video.transform.offset_x;
}

} // namespace

void run_transition_tests() {
    describe("timeline transitions", {
        it("dissolves across the cut, both clips playing past it", {
            auto fx = with_transition(TransitionKind::Dissolve, 10);
            expect(at(fx, 24).video.size()).toBe(1U);
            const Composition early = at(fx, 27); // 20% into [25, 35)
            expect(early.video.size()).toBe(2U);
            expect(layer_frame(early, 0)).toBe(27LL);
            expect(layer_frame(early, 1)).toBe(97LL); // B before its in point
            expect(near(early.video[0].opacity, 1.0) && near(early.video[1].opacity, 0.2))
                .toBeTruthy();
            const Composition late = at(fx, 34);
            expect(layer_frame(late, 0)).toBe(34LL); // A past its out point
            expect(layer_frame(late, 1)).toBe(104LL);
            expect(near(late.video[1].opacity, 0.9)).toBeTruthy();
            expect(at(fx, 35).video.size()).toBe(1U);
        });

        it("dips through black and wipes from the left", {
            auto dip = with_transition(TransitionKind::DipToBlack, 10);
            const Composition out = at(dip, 27);
            expect(out.video.size()).toBe(1U);
            expect(layer_frame(out, 0)).toBe(27LL);
            expect(near(out.video[0].opacity, 0.6)).toBeTruthy();
            const Composition in = at(dip, 33);
            expect(in.video.size()).toBe(1U);
            expect(layer_frame(in, 0)).toBe(103LL);
            expect(near(in.video[0].opacity, 0.6)).toBeTruthy();
            auto wipe = with_transition(TransitionKind::Wipe, 10);
            const Composition half = at(wipe, 30);
            expect(half.video.size()).toBe(2U);
            expect(near(half.video[1].reveal, 0.5) && near(half.video[1].opacity, 1.0))
                .toBeTruthy();
        });

        it("shortens to the media the clips have to spare", {
            // B starts 2 frames into its media: the transition can only span 28 to 32.
            auto fx = with_transition(TransitionKind::Dissolve, 10, 2);
            // At 28 the incoming clip has opacity 0 and is left out; at 29 it is a quarter in.
            expect(at(fx, 28).video.size()).toBe(1U);
            const Composition c = at(fx, 29);
            expect(c.video.size()).toBe(2U);
            expect(layer_frame(c, 1)).toBe(1LL);
            expect(near(c.video[1].opacity, 0.25)).toBeTruthy();
            expect(at(fx, 32).video.size()).toBe(1U);
        });

        it("is a plain cut without a clip ending at the start", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 20);
            const ClipId b = place(fx, fx.video, 30, 100, 30);
            expect(fx.editor
                       .execute(edit::set_transition(
                           b, Transition{.kind = TransitionKind::Dissolve, .duration = f(10)}))
                       .has_value())
                .toBeTruthy();
            expect(at(fx, 31).video.size()).toBe(1U);
            const Track& track = *fx.editor.timeline().find_track(fx.video);
            expect(transition_window(fx.editor.timeline(), track, 1).has_value()).toBeFalsy();
        });

        it("rejects empty transitions and keeps them off new cuts", {
            auto fx = with_transition(TransitionKind::Dissolve, 10);
            const ClipId b = fx.editor.timeline().clip_at(fx.video, 30)->id;
            expect(fx.editor
                       .execute(edit::set_transition(
                           b, Transition{.kind = TransitionKind::Dissolve, .duration = f(0)}))
                       .has_value())
                .toBeFalsy();
            // Splitting B: the right part starts at a new cut, without a transition.
            expect(fx.editor.execute(edit::split(b, f(45))).has_value()).toBeTruthy();
            expect(fx.editor.timeline().clip_at(fx.video, 50)->transition_in.has_value())
                .toBeFalsy();
            expect(fx.editor.timeline().find_clip(b)->transition_in.has_value()).toBeTruthy();
            expect(fx.editor.execute(edit::set_transition(b, std::nullopt)).has_value())
                .toBeTruthy();
            expect(at(fx, 31).video.size()).toBe(1U);
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(at(fx, 31).video.size()).toBe(2U);
        });
    });
}

namespace {

void keys_interpolate() {
    ClipId id;
    auto fx = keyed(Interpolation::Linear, &id);
    expect(near(x_at(fx, 0), 0.0)).toBeTruthy();
    expect(near(x_at(fx, 10), 25.0)).toBeTruthy();
    expect(near(x_at(fx, 50), 100.0)).toBeTruthy();
    // Scale moves geometrically: halfway from 1 to 4 is 2, a steady zoom.
    const Transform half = at(fx, 20).video.at(0).video.transform;
    expect(near(half.scale_x, 2.0) && near(half.scale_y, 2.0)).toBeTruthy();
    expect(at(fx, 20).video.at(0).video.transform_keys.empty()).toBeTruthy();
}

void keys_ease_and_hold() {
    ClipId id;
    auto eased = keyed(Interpolation::Ease, &id);
    expect(near(x_at(eased, 10), 100.0 * 0.15625)).toBeTruthy(); // smoothstep(1/4)
    expect(near(x_at(eased, 20), 50.0)).toBeTruthy();
    auto held = keyed(Interpolation::Hold, &id);
    expect(near(x_at(held, 39), 0.0) && near(x_at(held, 40), 100.0)).toBeTruthy();
}

void keys_follow_the_picture() {
    ClipId id;
    auto fx = keyed(Interpolation::Linear, &id);
    const double before = x_at(fx, 30);
    expect(fx.editor.execute(edit::split(id, f(20))).has_value()).toBeTruthy();
    expect(near(x_at(fx, 30), before)).toBeTruthy();
    expect(fx.editor.execute(edit::trim_start(id, f(10), false)).has_value()).toBeTruthy();
    expect(near(x_at(fx, 10), 25.0)).toBeTruthy();
    // Slipping by 8 frames shows the picture 8 frames later, and its motion with it.
    expect(fx.editor.execute(edit::slip(id, f(8))).has_value()).toBeTruthy();
    expect(near(x_at(fx, 10), 45.0)).toBeTruthy();
}

void keys_are_validated() {
    auto fx = make_fixture();
    const ClipId id = place(fx, fx.video, 0, 0, 30);
    VideoProperties v;
    v.transform_keys = {key(10, 0, 1), key(10, 5, 1)};
    expect(fx.editor.execute(edit::set_video(id, v)).has_value()).toBeFalsy();
    v.transform_keys = {key(0, 0, 1), key(10, 5, 0)};
    expect(fx.editor.execute(edit::set_video(id, v)).has_value()).toBeFalsy();
}

} // namespace

void run_keyframe_tests() {
    describe("timeline keyframes", {
        it("interpolates the transform between keys and holds it outside them",
           { keys_interpolate(); });
        it("eases and holds", { keys_ease_and_hold(); });
        it("keeps the motion on the picture through split, trim and slip",
           { keys_follow_the_picture(); });
        it("rejects keys out of order or with an invalid transform", { keys_are_validated(); });
    });
}

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
