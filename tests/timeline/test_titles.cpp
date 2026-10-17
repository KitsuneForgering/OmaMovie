#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include <string>

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

Title hello() {
    Title t;
    t.text = "Hello";
    return t;
}

edit::ClipSource title_source(std::int64_t duration, Title title = hello()) {
    return edit::ClipSource{.media = MediaId{},
                            .source_in = f(0),
                            .duration = f(duration),
                            .time_map = {},
                            .video = {},
                            .audio = {},
                            .title = std::move(title)};
}

// A title placed with overwrite on `track`; invalid ID if the edit failed.
ClipId place_title(Fixture& fx, TrackId track, std::int64_t at, std::int64_t duration,
                   Title title = hello()) {
    const ClipId id = fx.editor.new_clip_id();
    return fx.editor.execute(
               edit::overwrite(track, id, f(at), title_source(duration, std::move(title))))
               ? id
               : ClipId{};
}

bool valid(const Fixture& fx) {
    return fx.editor.timeline().validate().has_value();
}

} // namespace

void run_title_tests() {
    describe("Title clips (ADR-0015)", {
        it("sit on a video track without media", {
            auto fx = make_fixture();
            const ClipId t = place_title(fx, fx.overlay, 10, 30);
            expect(t.valid()).toBeTruthy();
            expect(valid(fx)).toBeTruthy();
        });

        it("are refused on an audio track, with media, or with bad text parameters", {
            auto fx = make_fixture();
            expect(place_title(fx, fx.audio, 0, 30).valid()).toBeFalsy();
            Title big = hello();
            big.size = 0.9;
            expect(place_title(fx, fx.overlay, 0, 30, big).valid()).toBeFalsy();
            Title long_text = hello();
            long_text.text = std::string(kMaxTitleBytes + 1, 'x');
            expect(place_title(fx, fx.overlay, 0, 30, long_text).valid()).toBeFalsy();
            Title dark = hello();
            dark.color[3] = 2.0F;
            expect(place_title(fx, fx.overlay, 0, 30, dark).valid()).toBeFalsy();
            auto with_media = title_source(30);
            with_media.media = kCamera;
            expect(
                fx.editor
                    .execute(edit::overwrite(fx.overlay, fx.editor.new_clip_id(), f(0), with_media))
                    .has_value())
                .toBeFalsy();
            expect(valid(fx)).toBeTruthy();
        });

        it("evaluate to a layer carrying the title and no sound", {
            auto fx = make_fixture();
            place(fx, fx.video, 0, 0, 60);
            place_title(fx, fx.overlay, 10, 20);
            const Composition c = evaluate(fx.editor.timeline(), f(15)).value();
            expect(static_cast<int>(c.video.size())).toBe(2);
            expect(c.video.at(1).title.has_value()).toBeTruthy();
            expect(c.video.at(1).title->text == "Hello").toBeTruthy();
            expect(c.video.at(0).title.has_value()).toBeFalsy();
            expect(static_cast<int>(c.audio.size())).toBe(1); // the camera only
        });

        it("trim past any length and split like other clips", {
            auto fx = make_fixture();
            const ClipId t = place_title(fx, fx.overlay, 10, 20);
            expect(fx.editor.execute(edit::trim_end(t, f(500), false)).has_value()).toBeTruthy();
            expect(fx.editor.execute(edit::trim_start(t, f(0), false)).has_value()).toBeTruthy();
            expect(fx.editor.execute(edit::split(t, f(100))).has_value()).toBeTruthy();
            const auto& clips = fx.editor.timeline().find_track(fx.overlay)->clips;
            expect(static_cast<int>(clips.size())).toBe(2);
            expect(clips.at(1).title.has_value()).toBeTruthy();
            expect(valid(fx)).toBeTruthy();
        });

        it("change text with one undoable edit; media clips refuse it", {
            auto fx = make_fixture();
            const ClipId t = place_title(fx, fx.overlay, 0, 30);
            const ClipId cam = place(fx, fx.video, 0, 0, 30);
            Title next = hello();
            next.text = "Chapter 2";
            next.placement = TitlePlacement::Center;
            expect(fx.editor.execute(edit::set_title(t, next)).has_value()).toBeTruthy();
            expect(fx.editor.timeline().find_clip(t)->title->text == "Chapter 2").toBeTruthy();
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.timeline().find_clip(t)->title->text == "Hello").toBeTruthy();
            expect(fx.editor.execute(edit::set_title(cam, next)).has_value()).toBeFalsy();
        });

        it("connect to the storyline and follow it", {
            auto fx = make_fixture();
            const ClipId a = place(fx, fx.video, 0, 0, 30);
            const ClipId t = place_title(fx, fx.overlay, 10, 10);
            expect(fx.editor.execute(edit::connect(t, a)).has_value()).toBeTruthy();
            // A clip inserted before the storyline clip pushes it 10 frames; the title follows.
            expect(
                fx.editor
                    .execute(edit::insert(fx.video, fx.editor.new_clip_id(), f(0), camera(0, 10)))
                    .has_value())
                .toBeTruthy();
            expect(fx.editor.timeline().find_clip(t)->start_ticks()).toBe(20LL);
            expect(valid(fx)).toBeTruthy();
        });
    });
}
