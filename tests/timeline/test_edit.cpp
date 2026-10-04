#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include <string>
#include <vector>

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

bool ok(Fixture& fx, std::unique_ptr<Command> command) {
    return fx.editor.execute(std::move(command)).has_value();
}

// Three adjacent clips: 0+10@0 10+10@50 20+10@100.
Fixture three_clips() {
    Fixture fx = make_fixture();
    place(fx, fx.video, 0, 0, 10);
    place(fx, fx.video, 10, 50, 10);
    place(fx, fx.video, 20, 100, 10);
    return fx;
}

ClipId clip_at(const Fixture& fx, std::int64_t frame) {
    const Clip* c = fx.editor.timeline().clip_at(fx.video, frame);
    return c != nullptr ? c->id : ClipId{};
}

// Applies every command, recording the layout after each, then undoes and redoes all of them
// checking that every intermediate layout comes back exactly.
bool undo_redo_round_trip(Fixture& fx, std::vector<std::unique_ptr<Command>> commands) {
    std::vector<std::string> states{layout(fx)};
    for (auto& c : commands) {
        if (!fx.editor.execute(std::move(c))) {
            return false;
        }
        states.push_back(layout(fx));
    }
    for (std::size_t i = states.size() - 1; i > 0; --i) {
        if (!fx.editor.undo() || layout(fx) != states[i - 1]) {
            return false;
        }
    }
    for (std::size_t i = 1; i < states.size(); ++i) {
        if (!fx.editor.redo() || layout(fx) != states[i]) {
            return false;
        }
    }
    return !fx.editor.can_redo();
}

std::unique_ptr<Command> overwrite_music(TrackId track, ClipId id, std::int64_t at) {
    return edit::overwrite(
        track, id, f(at),
        edit::ClipSource{.media = kMusic,
                         .source_in = oma::RationalTime::make(0, q(1, 48000)).value(),
                         .duration = f(10),
                         .time_map = {},
                         .video = {},
                         .audio = {}});
}

MediaInfo camera_media() {
    return MediaInfo{.id = kCamera,
                     .start = mf(0),
                     .duration = mf(30),
                     .has_video = true,
                     .has_audio = false,
                     .still = false};
}

} // namespace

void run_edit_tests() {
    describe("timeline edits: placing clips", {
        it("appends clips back to back", {
            auto fx = make_fixture();
            const TrackId v = fx.video;
            expect(ok(fx, edit::append(v, fx.editor.new_clip_id(), camera(0, 10)))).toBeTruthy();
            expect(ok(fx, edit::append(v, fx.editor.new_clip_id(), camera(30, 5)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+5@30");
        });

        it("overwrites the middle of a clip by splitting it", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 30);
            expect(place(fx, fx.video, 10, 100, 5).valid()).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+5@100 15+15@15");
        });

        it("overwrites across an edit point by trimming both sides", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 10);
            place(fx, fx.video, 10, 50, 10);
            place(fx, fx.video, 5, 200, 10);
            expect(layout(fx)).toEqual("0+5@0 5+10@200 15+5@55");
        });

        it("inserts inside a clip by splitting it and pushing the rest", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 30);
            place(fx, fx.video, 40, 200, 5);
            const ClipId id = fx.editor.new_clip_id();
            expect(ok(fx, edit::insert(fx.video, id, f(10), camera(100, 5)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+5@100 15+20@10 45+5@200");
        });

        it("inserts at an edit point without splitting", {
            auto fx = three_clips();
            expect(ok(fx, edit::insert(fx.video, fx.editor.new_clip_id(), f(10), camera(200, 5))))
                .toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+5@200 15+10@50 25+10@100");
        });

        it("rejects media of the wrong kind and leaves the timeline unchanged", {
            auto fx = three_clips();
            const auto before = layout(fx);
            const auto revision = fx.editor.revision();
            expect(ok(fx, overwrite_music(fx.video, fx.editor.new_clip_id(), 0))).toBeFalsy();
            expect(layout(fx)).toEqual(before);
            expect(fx.editor.revision()).toBe(revision);
            expect(std::string(fx.editor.undo_name())).toEqual("Overwrite");
        });

        it("accepts audio-only media on an audio track", {
            auto fx = make_fixture();
            expect(ok(fx, overwrite_music(fx.audio, fx.editor.new_clip_id(), 0))).toBeTruthy();
            expect(fx.editor.timeline().find_track(fx.audio)->clips.size()).toBe(1U);
        });

        it("rejects a source range beyond the media", {
            auto fx = make_fixture();
            expect(place(fx, fx.video, 0, 295, 10).valid()).toBeFalsy();
            expect(place(fx, fx.video, 0, 290, 10).valid()).toBeTruthy();
        });

        it("rejects reusing a clip ID", {
            auto fx = make_fixture();
            const ClipId id = fx.editor.new_clip_id();
            expect(ok(fx, edit::overwrite(fx.video, id, f(0), camera(0, 5)))).toBeTruthy();
            expect(ok(fx, edit::overwrite(fx.overlay, id, f(0), camera(0, 5)))).toBeFalsy();
        });

        it("rejects times off the sequence grid", {
            auto fx = make_fixture();
            const auto half_frame = oma::RationalTime::make(1, q(1, 60)).value();
            expect(ok(fx,
                      edit::overwrite(fx.video, fx.editor.new_clip_id(), half_frame, camera(0, 5))))
                .toBeFalsy();
        });
    });

    describe("timeline edits: deleting and splitting", {
        it("deletes a clip and leaves a gap", {
            auto fx = three_clips();
            expect(ok(fx, edit::remove_clip(clip_at(fx, 10)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 20+10@100");
        });

        it("ripple deletes a clip and closes the gap", {
            auto fx = three_clips();
            expect(ok(fx, edit::ripple_delete(clip_at(fx, 10)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+10@100");
        });

        it("ripple deletes a range across clips", {
            auto fx = three_clips();
            expect(ok(fx, edit::ripple_delete_range(fx.video, f(5), f(25)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+5@0 5+5@105");
        });

        it("ripple deletes a range inside one clip", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 30);
            expect(ok(fx, edit::ripple_delete_range(fx.video, f(10), f(20)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+10@20");
        });

        it("splits a clip and gives the right part a new ID", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 0, 0, 30);
            expect(ok(fx, edit::split(id, f(12)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+12@0 12+18@12");
            expect(clip_at(fx, 0) == id).toBeTruthy();
            expect(clip_at(fx, 12) != id && clip_at(fx, 12).valid()).toBeTruthy();
        });

        it("refuses to split at a clip edge", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 0, 0, 30);
            expect(ok(fx, edit::split(id, f(0)))).toBeFalsy();
            expect(ok(fx, edit::split(id, f(30)))).toBeFalsy();
        });
    });

    describe("timeline edits: trimming", {
        it("trims the start in place, moving the source in", {
            auto fx = three_clips();
            expect(ok(fx, edit::remove_clip(clip_at(fx, 0)))).toBeTruthy();
            expect(ok(fx, edit::trim_start(clip_at(fx, 10), f(13), false))).toBeTruthy();
            expect(layout(fx)).toEqual("13+7@53 20+10@100");
            expect(ok(fx, edit::trim_start(clip_at(fx, 13), f(4), false))).toBeTruthy();
            expect(layout(fx)).toEqual("4+16@44 20+10@100");
        });

        it("ripple trims the start, pulling the following clips", {
            auto fx = three_clips();
            expect(ok(fx, edit::trim_start(clip_at(fx, 10), f(13), true))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+7@53 17+10@100");
        });

        it("ripple trims the end outward, pushing the following clips", {
            auto fx = three_clips();
            expect(ok(fx, edit::trim_end(clip_at(fx, 10), f(25), true))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+15@50 25+10@100");
        });

        it("refuses to trim into a neighbour without ripple", {
            auto fx = three_clips();
            expect(ok(fx, edit::trim_end(clip_at(fx, 10), f(25), false))).toBeFalsy();
            expect(layout(fx)).toEqual("0+10@0 10+10@50 20+10@100");
        });

        it("refuses to trim before the start of the media", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 10, 3, 10);
            expect(ok(fx, edit::trim_start(id, f(6), false))).toBeFalsy();
            expect(ok(fx, edit::trim_start(id, f(7), false))).toBeTruthy();
            expect(layout(fx)).toEqual("7+13@0");
        });

        it("rolls an edit point both ways", {
            auto fx = three_clips();
            const ClipId left = clip_at(fx, 0);
            expect(ok(fx, edit::roll(left, f(13)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+13@0 13+7@53 20+10@100");
            expect(ok(fx, edit::roll(left, f(8)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+8@0 8+12@48 20+10@100");
        });

        it("refuses to roll without an adjacent clip", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 0, 0, 10);
            place(fx, fx.video, 12, 50, 10);
            expect(ok(fx, edit::roll(id, f(11)))).toBeFalsy();
        });

        it("slips the source under a fixed range", {
            auto fx = three_clips();
            expect(ok(fx, edit::slip(clip_at(fx, 10), f(5)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+10@55 20+10@100");
            expect(ok(fx, edit::slip(clip_at(fx, 0), f(-1)))).toBeFalsy();
        });

        it("slides a clip between its neighbours", {
            auto fx = three_clips();
            expect(ok(fx, edit::slide(clip_at(fx, 10), f(3)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+13@0 13+10@50 23+7@103");
            expect(ok(fx, edit::slide(clip_at(fx, 13), f(-5)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+8@0 8+10@50 18+12@98");
        });

        it("slides into a gap without touching the far clip", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 10);
            const ClipId id = place(fx, fx.video, 10, 50, 10);
            place(fx, fx.video, 25, 100, 10);
            expect(ok(fx, edit::slide(id, f(4)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+14@0 14+10@50 25+10@100");
            expect(ok(fx, edit::slide(id, f(2)))).toBeFalsy();
        });
    });

    describe("timeline edits: moving, speed and properties", {
        it("moves a clip to a free spot on another track", {
            auto fx = three_clips();
            const ClipId id = clip_at(fx, 10);
            expect(ok(fx, edit::move_clip(id, fx.overlay, f(40)))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 20+10@100");
            expect(layout(fx.editor.timeline(), fx.overlay)).toEqual("40+10@50");
        });

        it("refuses to move onto another clip or onto a track of another kind", {
            auto fx = three_clips();
            const ClipId id = clip_at(fx, 10);
            expect(ok(fx, edit::move_clip(id, fx.video, f(15)))).toBeFalsy();
            expect(ok(fx, edit::move_clip(id, fx.audio, f(40)))).toBeFalsy();
            expect(layout(fx)).toEqual("0+10@0 10+10@50 20+10@100");
        });

        it("doubles the speed, halving the duration and pulling later clips", {
            auto fx = three_clips();
            expect(ok(fx, edit::set_speed(clip_at(fx, 10), q(2, 1), true))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+5@50 15+10@100");
        });

        it("halves the speed, doubling the duration and pushing later clips", {
            auto fx = three_clips();
            expect(ok(fx, edit::set_speed(clip_at(fx, 10), q(1, 2), true))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+20@50 30+10@100");
            // Trimming a slowed clip moves the source by half as much.
            expect(ok(fx, edit::trim_start(clip_at(fx, 10), f(14), true))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+16@52 26+10@100");
        });

        it("rounds an uneven speed down to the sequence grid", {
            auto fx = three_clips();
            // 10 frames at 3x is 3.33 frames: 3 frames, staying inside the source range.
            expect(ok(fx, edit::set_speed(clip_at(fx, 10), q(3, 1), true))).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+3@50 13+10@100");
        });

        it("rejects a non-positive speed", {
            auto fx = three_clips();
            expect(ok(fx, edit::set_speed(clip_at(fx, 10), q(0, 1), true))).toBeFalsy();
        });

        it("changes video and audio properties, rejecting invalid ones", {
            auto fx = three_clips();
            const ClipId id = clip_at(fx, 0);
            VideoProperties v;
            v.opacity = 0.5F;
            v.transform.scale_x = 0.5;
            expect(ok(fx, edit::set_video(id, v))).toBeTruthy();
            expect(fx.editor.timeline().find_clip(id)->video == v).toBeTruthy();
            v.opacity = 1.5F;
            expect(ok(fx, edit::set_video(id, v))).toBeFalsy();
            v.opacity = 1.0F;
            v.color.exposure = 1.5;
            v.filter.kind = FilterKind::Sepia;
            v.filter.amount = 0.5;
            expect(ok(fx, edit::set_video(id, v))).toBeTruthy();
            v.color.saturation = -1.5;
            expect(ok(fx, edit::set_video(id, v))).toBeFalsy();
            v.color.saturation = 0.0;
            v.filter.amount = 2.0;
            expect(ok(fx, edit::set_video(id, v))).toBeFalsy();
            AudioProperties a;
            a.fade_in = f(4);
            a.fade_out = f(6);
            expect(ok(fx, edit::set_audio(id, a))).toBeTruthy();
            a.fade_out = f(7);
            expect(ok(fx, edit::set_audio(id, a))).toBeFalsy();
            a.fade_out = f(6);
            a.eq.mid_db = -24.0F;
            a.noise.amount = 0.5F;
            a.noise.floor_db = -40.0F;
            expect(ok(fx, edit::set_audio(id, a))).toBeTruthy();
            a.eq.high_db = 25.0F;
            expect(ok(fx, edit::set_audio(id, a))).toBeFalsy();
            a.eq.high_db = 0.0F;
            a.noise.amount = 1.5F;
            expect(ok(fx, edit::set_audio(id, a))).toBeFalsy();
            a.noise.amount = 1.0F;
            a.noise.floor_db = 3.0F;
            expect(ok(fx, edit::set_audio(id, a))).toBeFalsy();
        });
    });

    describe("timeline edits: history", {
        it("undoes and redoes every operation exactly", {
            auto fx = three_clips();
            std::vector<std::unique_ptr<Command>> commands;
            commands.push_back(edit::split(clip_at(fx, 10), f(14)));
            commands.push_back(
                edit::insert(fx.video, fx.editor.new_clip_id(), f(5), camera(200, 4)));
            commands.push_back(edit::roll(clip_at(fx, 0), f(3)));
            commands.push_back(edit::trim_end(clip_at(fx, 20), f(30), true));
            commands.push_back(edit::ripple_delete_range(fx.video, f(2), f(7)));
            commands.push_back(edit::set_speed(clip_at(fx, 0), q(1, 2), true));
            expect(undo_redo_round_trip(fx, std::move(commands))).toBeTruthy();
        });

        it("records each compound operation as one history entry", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 30);
            place(fx, fx.video, 10, 100, 5);
            expect(std::string(fx.editor.undo_name())).toEqual("Overwrite");
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(layout(fx)).toEqual("0+30@0");
        });

        it("groups commands in a transaction and rolls all back on failure", {
            auto fx = three_clips();
            std::vector<std::unique_ptr<Command>> good;
            good.push_back(edit::remove_clip(clip_at(fx, 0)));
            good.push_back(edit::remove_clip(clip_at(fx, 20)));
            expect(ok(fx, edit::transaction("Delete Selection", std::move(good)))).toBeTruthy();
            expect(layout(fx)).toEqual("10+10@50");
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+10@50 20+10@100");

            std::vector<std::unique_ptr<Command>> bad;
            bad.push_back(edit::remove_clip(clip_at(fx, 0)));
            bad.push_back(edit::split(clip_at(fx, 10), f(10)));
            expect(ok(fx, edit::transaction("Broken", std::move(bad)))).toBeFalsy();
            expect(layout(fx)).toEqual("0+10@0 10+10@50 20+10@100");
        });

        it("clears the redo stack on a new edit", {
            auto fx = three_clips();
            expect(ok(fx, edit::remove_clip(clip_at(fx, 0)))).toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.can_redo()).toBeTruthy();
            expect(ok(fx, edit::remove_clip(clip_at(fx, 20)))).toBeTruthy();
            expect(fx.editor.can_redo()).toBeFalsy();
            expect(fx.editor.redo().has_value()).toBeFalsy();
        });

        it("restores a removed track with its clips", {
            auto fx = three_clips();
            expect(ok(fx, edit::remove_track(fx.video))).toBeTruthy();
            expect(fx.editor.timeline().find_track(fx.video) == nullptr).toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(layout(fx)).toEqual("0+10@0 10+10@50 20+10@100");
            expect(fx.editor.timeline().tracks().front().id == fx.video).toBeTruthy();
        });

        it("registers media outside the history", {
            Editor ed(Timeline::create(oma::frame_rates::k30, q(1, 30)).value());
            const MediaInfo cam = camera_media();
            expect(ed.add_media(cam).has_value()).toBeTruthy();
            expect(ed.can_undo()).toBeFalsy();
            expect(ed.add_media(cam).has_value()).toBeFalsy();
            expect(ed.timeline().find_media(kCamera) != nullptr).toBeTruthy();
        });

        it("forgets the history but keeps the timeline", {
            auto fx = three_clips();
            expect(fx.editor.can_undo()).toBeTruthy();
            fx.editor.clear_history();
            expect(fx.editor.can_undo() || fx.editor.can_redo()).toBeFalsy();
            expect(layout(fx)).toEqual("0+10@0 10+10@50 20+10@100");
        });

        it("never reuses an ID after undo", {
            auto fx = make_fixture();
            const ClipId id = place(fx, fx.video, 0, 0, 30);
            expect(ok(fx, edit::split(id, f(10)))).toBeTruthy();
            const ClipId first_tail = clip_at(fx, 10);
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(ok(fx, edit::split(id, f(10)))).toBeTruthy();
            expect(clip_at(fx, 10) != first_tail).toBeTruthy();
        });

        it("bumps the revision on execute, undo and redo", {
            auto fx = three_clips();
            const auto r0 = fx.editor.revision();
            expect(ok(fx, edit::remove_clip(clip_at(fx, 0)))).toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.redo().has_value()).toBeTruthy();
            expect(fx.editor.revision() - r0).toBe(3U);
        });

        it("adds, sorts and removes markers through the history", {
            auto fx = make_fixture();
            const MarkerId late = fx.editor.new_marker_id();
            const MarkerId early = fx.editor.new_marker_id();
            expect(ok(fx, edit::add_marker(late, f(90), "chorus"))).toBeTruthy();
            expect(ok(fx, edit::add_marker(early, f(30), "intro"))).toBeTruthy();
            expect(fx.editor.timeline().markers().front().name).toEqual("intro");
            expect(ok(fx, edit::remove_marker(early))).toBeTruthy();
            expect(fx.editor.timeline().markers().size()).toBe(1U);
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.timeline().markers().size()).toBe(2U);
            const auto off_grid = oma::RationalTime::make(1, q(1, 60)).value();
            expect(ok(fx, edit::add_marker(fx.editor.new_marker_id(), off_grid, "x"))).toBeFalsy();
        });
    });

    describe("timeline edits: detaching audio", {
        it("moves a video clip's sound to an audio clip of its own", {
            auto fx = make_fixture();
            const ClipId video = place(fx, fx.video, 10, 5, 30);
            const ClipId sound = fx.editor.new_clip_id();
            expect(ok(fx, edit::detach_audio(video, fx.audio, sound))).toBeTruthy();
            const Timeline& tl = fx.editor.timeline();
            const Clip* a = tl.find_clip(sound);
            expect(a != nullptr && tl.track_of(sound)->id == fx.audio).toBeTruthy();
            expect(a->start == f(10) && a->duration == f(30) && a->source_in == mf(5)).toBeTruthy();
            expect(tl.find_clip(video)->audio_detached).toBeTruthy();
            // The sound plays once, from the lane.
            const auto c = evaluate(tl, f(20)).value();
            expect(c.audio.size()).toBe(1U);
            expect(c.audio.front().clip == sound).toBeTruthy();
            expect(c.video.size()).toBe(1U);
        });

        it("lets the two parts be trimmed apart and joins them on undo", {
            auto fx = make_fixture();
            const ClipId video = place(fx, fx.video, 10, 5, 30);
            const ClipId sound = fx.editor.new_clip_id();
            expect(ok(fx, edit::detach_audio(video, fx.audio, sound))).toBeTruthy();
            // A J-cut: the sound starts 3 frames before the picture.
            expect(ok(fx, edit::trim_start(sound, f(7), false))).toBeTruthy();
            expect(layout(fx.editor.timeline(), fx.audio)).toEqual("7+33@2");
            expect(layout(fx)).toEqual("10+30@5");
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.timeline().find_clip(sound) == nullptr).toBeTruthy();
            expect(fx.editor.timeline().find_clip(video)->audio_detached).toBeFalsy();
            expect(evaluate(fx.editor.timeline(), f(20)).value().audio.size()).toBe(1U);
        });

        it("refuses clips without sound, occupied lanes and a second detach", {
            auto fx = make_fixture();
            const ClipId video = place(fx, fx.video, 0, 0, 30);
            const ClipId picture = fx.editor.new_clip_id();
            expect(fx.editor
                       .execute(edit::overwrite(fx.overlay, picture, f(0),
                                                edit::ClipSource{.media = kPicture,
                                                                 .source_in = {},
                                                                 .duration = f(10),
                                                                 .time_map = {},
                                                                 .video = {},
                                                                 .audio = {}}))
                       .has_value())
                .toBeTruthy();
            expect(ok(fx, edit::detach_audio(picture, fx.audio, fx.editor.new_clip_id())))
                .toBeFalsy();
            expect(ok(fx, overwrite_music(fx.audio, fx.editor.new_clip_id(), 5))).toBeTruthy();
            expect(ok(fx, edit::detach_audio(video, fx.audio, fx.editor.new_clip_id())))
                .toBeFalsy();
            expect(ok(fx, edit::detach_audio(video, fx.video, fx.editor.new_clip_id())))
                .toBeFalsy();
            expect(fx.editor.undo().has_value()).toBeTruthy(); // the music
            expect(ok(fx, edit::detach_audio(video, fx.audio, fx.editor.new_clip_id())))
                .toBeTruthy();
            expect(ok(fx, edit::detach_audio(video, fx.audio, fx.editor.new_clip_id())))
                .toBeFalsy();
        });

        it("keeps the detached state when the video clip is split", {
            auto fx = make_fixture();
            const ClipId video = place(fx, fx.video, 0, 0, 30);
            expect(ok(fx, edit::detach_audio(video, fx.audio, fx.editor.new_clip_id())))
                .toBeTruthy();
            expect(ok(fx, edit::split(video, f(10)))).toBeTruthy();
            expect(fx.editor.timeline().find_clip(clip_at(fx, 20))->audio_detached).toBeTruthy();
        });
    });
}
