#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

std::int64_t default_den(oma::FrameRate rate, int hz) {
    return Timeline::default_timebase(rate, oma::SampleRate::make(hz).value()).value().den();
}

// A 23.976 fps sequence on its default timebase with a media file in a 1/15360 timebase (a
// common MP4 video timescale that is not a multiple of the frame grid).
struct OddGrid {
    Editor editor;
    TrackId track;
    ClipId clip;
};

OddGrid odd_grid() {
    const auto rate = oma::frame_rates::k23_976;
    OddGrid g{Editor(Timeline::create(rate, oma::SampleRate::make(48000).value()).value()), {}, {}};
    const auto tb = q(1, 15360);
    (void)g.editor.add_media(MediaInfo{.id = kCamera,
                                       .start = oma::RationalTime::make(0, tb).value(),
                                       .duration = oma::RationalTime::make(15360 * 60, tb).value(),
                                       .has_video = true,
                                       .has_audio = true,
                                       .still = false});
    g.track = g.editor.new_track_id();
    (void)g.editor.execute(edit::add_track(g.track, TrackKind::Video, "V1"));
    g.clip = g.editor.new_clip_id();
    const Timeline& tl = g.editor.timeline();
    const auto frame = rate.frame_to_time(1); // 1001/24000 s
    const auto ten_frames = tl.at(tl.to_ticks(frame).value() * 10);
    (void)g.editor.execute(
        edit::overwrite(g.track, g.clip, tl.at(0),
                        edit::ClipSource{.media = kCamera,
                                         .source_in = oma::RationalTime::make(15360, tb).value(),
                                         .duration = ten_frames,
                                         .time_map = {},
                                         .video = {},
                                         .audio = {}}));
    return g;
}

// Trims the start forward by `ticks` and back again; the source in must return exactly.
bool trim_round_trip_is_exact(std::int64_t ticks) {
    OddGrid g = odd_grid();
    const Timeline& tl = g.editor.timeline();
    const auto original = tl.find_clip(g.clip)->source_in;
    if (!g.editor.execute(edit::trim_start(g.clip, tl.at(ticks), false))) {
        return false;
    }
    const bool moved = tl.find_clip(g.clip)->source_in != original;
    if (!g.editor.execute(edit::trim_start(g.clip, tl.at(0), false))) {
        return false;
    }
    return moved && tl.find_clip(g.clip)->source_in == original;
}

} // namespace

void run_model_tests() {
    describe("Timeline model", {
        it("picks a timebase that holds frames and samples", {
            expect(default_den(oma::frame_rates::k29_97, 48000)).toBe(240000LL);
            expect(default_den(oma::frame_rates::k23_976, 44100)).toBe(3528000LL);
            expect(default_den(oma::frame_rates::k59_94, 96000)).toBe(480000LL);
            expect(default_den(oma::frame_rates::k25, 48000)).toBe(48000LL);
            const auto tl =
                Timeline::create(oma::frame_rates::k29_97, oma::SampleRate::make(48000).value());
            expect(tl->to_ticks(oma::frame_rates::k29_97.frame_to_time(1)).value()).toBe(8008LL);
            expect(tl->to_ticks(oma::SampleRate::make(48000).value().sample_to_time(1)).value())
                .toBe(5LL);
        });

        it("rejects a timebase that does not hold whole frames", {
            expect(Timeline::create(oma::frame_rates::k29_97, q(1, 30)).has_value()).toBeFalsy();
            expect(Timeline::create(oma::frame_rates::k30, q(1, 30)).has_value()).toBeTruthy();
        });

        it("converts exact times from other timebases and rejects inexact ones", {
            const auto tl = Timeline::create(oma::frame_rates::k30, q(1, 30)).value();
            const auto exact = oma::RationalTime::make(6000, q(1, 90000)).value();
            const auto inexact = oma::RationalTime::make(1, q(1, 48000)).value();
            expect(tl.to_ticks(exact).value()).toBe(2LL);
            expect(tl.to_ticks(inexact).has_value()).toBeFalsy();
        });

        it("finds clips by half-open ranges", {
            auto fx = make_fixture();
            const ClipId a = place(fx, fx.video, 0, 0, 10);
            const ClipId b = place(fx, fx.video, 10, 50, 10);
            const Timeline& tl = fx.editor.timeline();
            expect(tl.clip_at(fx.video, 9)->id == a).toBeTruthy();
            expect(tl.clip_at(fx.video, 10)->id == b).toBeTruthy();
            expect(tl.clip_at(fx.video, 20) == nullptr).toBeTruthy();
            expect(tl.track_of(b)->id == fx.video).toBeTruthy();
            expect(tl.duration() == f(20)).toBeTruthy();
            expect(tl.validate().has_value()).toBeTruthy();
        });

        it("rejects non-positive speeds", {
            expect(TimeMap::constant(q(0, 1)).has_value()).toBeFalsy();
            expect(TimeMap::constant(q(-1, 1)).has_value()).toBeFalsy();
            expect(TimeMap::constant(q(1, 2)).has_value()).toBeTruthy();
        });

        it("edits audio at sample positions between frames", {
            Editor ed(Timeline::create(oma::frame_rates::k30, oma::SampleRate::make(48000).value())
                          .value());
            (void)ed.add_media(
                MediaInfo{.id = kMusic,
                          .start = oma::RationalTime::make(0, q(1, 48000)).value(),
                          .duration = oma::RationalTime::make(48000, q(1, 48000)).value(),
                          .has_video = false,
                          .has_audio = true,
                          .still = false});
            const TrackId a = ed.new_track_id();
            (void)ed.execute(edit::add_track(a, TrackKind::Audio, "A1"));
            const ClipId id = ed.new_clip_id();
            const auto second = oma::RationalTime::make(1, q(1, 1)).value();
            const auto zero = oma::RationalTime::make(0, q(1, 48000)).value();
            expect(ed.execute(edit::overwrite(a, id, ed.timeline().at(0),
                                              edit::ClipSource{.media = kMusic,
                                                               .source_in = zero,
                                                               .duration = second,
                                                               .time_map = {},
                                                               .video = {},
                                                               .audio = {}}))
                       .has_value())
                .toBeTruthy();
            // Sample 1001 is not on the 30 fps frame grid (1600 samples per frame).
            const auto sample = oma::RationalTime::make(1001, q(1, 48000)).value();
            expect(ed.execute(edit::split(id, sample)).has_value()).toBeTruthy();
            const Clip* tail = ed.timeline().clip_at(a, ed.timeline().to_ticks(sample).value());
            expect(tail->source_in == sample).toBeTruthy();
        });

        it("keeps source positions exact through trims on a non-matching media grid", {
            expect(trim_round_trip_is_exact(1)).toBeTruthy();
            expect(trim_round_trip_is_exact(10010)).toBeTruthy();
        });

        it("rounds media times down to the media grid only on evaluation", {
            OddGrid g = odd_grid();
            const Timeline& tl = g.editor.timeline();
            // The sequence tick is 1/48000 s, 0.32 of a 1/15360 media tick: still 15360.
            auto at_one = evaluate(tl, tl.at(1));
            expect(at_one->video.front().media_time.value()).toBe(15360LL);
            // One frame (2002 ticks) is 640.64 media ticks: floor to 640.
            auto at_frame = evaluate(tl, tl.at(2002));
            expect(at_frame->video.front().media_time.value()).toBe(16000LL);
        });
    });
}
