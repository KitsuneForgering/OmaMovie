#include "fixture.hpp"

#include <string>
#include <vector>

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

Caption cue(CaptionId id, std::int64_t start, std::int64_t frames, std::string text) {
    return Caption{.id = id, .start = f(start), .duration = f(frames), .text = std::move(text)};
}

bool ok(Fixture& fx, std::unique_ptr<Command> c) {
    return fx.editor.execute(std::move(c)).has_value();
}

std::size_t count(const Fixture& fx) {
    return fx.editor.timeline().captions().size();
}

} // namespace

void run_caption_tests() {
    describe("Captions (ADR-0017)", {
        it("are added sorted and edited and removed as one undo entry each", {
            auto fx = make_fixture();
            const CaptionId a = fx.editor.new_caption_id();
            const CaptionId b = fx.editor.new_caption_id();
            expect(ok(fx, edit::add_caption(cue(b, 30, 30, "second")))).toBeTruthy();
            expect(ok(fx, edit::add_caption(cue(a, 0, 30, "first")))).toBeTruthy();
            expect(fx.editor.timeline().captions()[0].text).toBe(std::string("first"));
            expect(ok(fx, edit::set_caption(cue(a, 0, 20, "first, shorter")))).toBeTruthy();
            expect(fx.editor.timeline().captions()[0].duration == f(20)).toBeTruthy();
            expect(ok(fx, edit::remove_caption(b))).toBeTruthy();
            expect(count(fx)).toBe(1U);
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(count(fx)).toBe(2U);
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(fx.editor.timeline().captions()[0].text).toBe(std::string("first"));
        });

        it("refuse overlaps, off-grid times, empty durations and long text", {
            auto fx = make_fixture();
            expect(ok(fx, edit::add_caption(cue(fx.editor.new_caption_id(), 0, 30, "a"))))
                .toBeTruthy();
            expect(ok(fx, edit::add_caption(cue(fx.editor.new_caption_id(), 29, 10, "overlaps"))))
                .toBeFalsy();
            auto off = cue(fx.editor.new_caption_id(), 40, 10, "half a frame");
            off.start = oma::RationalTime::make(81, q(1, 60)).value();
            expect(ok(fx, edit::add_caption(off))).toBeFalsy();
            expect(ok(fx, edit::add_caption(cue(fx.editor.new_caption_id(), 40, 0, "empty"))))
                .toBeFalsy();
            expect(ok(fx, edit::add_caption(cue(fx.editor.new_caption_id(), 40, 10,
                                                std::string(kMaxCaptionBytes + 1, 'x')))))
                .toBeFalsy();
            expect(count(fx)).toBe(1U);
            expect(fx.editor.timeline().validate().has_value()).toBeTruthy();
        });

        it("are replaced wholesale by an import, undone exactly", {
            auto fx = make_fixture();
            expect(ok(fx, edit::add_caption(cue(fx.editor.new_caption_id(), 0, 30, "old"))))
                .toBeTruthy();
            std::vector<Caption> imported;
            imported.push_back(cue(fx.editor.new_caption_id(), 60, 15, "two"));
            imported.push_back(cue(fx.editor.new_caption_id(), 10, 15, "one"));
            expect(ok(fx, edit::replace_captions(imported))).toBeTruthy();
            expect(count(fx)).toBe(2U);
            expect(fx.editor.timeline().captions()[0].text).toBe(std::string("one"));
            expect(fx.editor.undo().has_value()).toBeTruthy();
            expect(count(fx)).toBe(1U);
            expect(fx.editor.timeline().captions()[0].text).toBe(std::string("old"));
        });

        it("survive a restore and new IDs continue after them", {
            auto fx = make_fixture();
            const CaptionId id = fx.editor.new_caption_id();
            expect(ok(fx, edit::add_caption(cue(id, 0, 30, "kept")))).toBeTruthy();
            const Timeline& t = fx.editor.timeline();
            auto back =
                Timeline::restore(t.frame_rate(), t.timebase(),
                                  std::vector<Track>(t.tracks().begin(), t.tracks().end()),
                                  std::vector<Marker>(t.markers().begin(), t.markers().end()),
                                  std::vector<MediaInfo>(t.media().begin(), t.media().end()),
                                  std::vector<LutInfo>(t.luts().begin(), t.luts().end()),
                                  std::vector<Caption>(t.captions().begin(), t.captions().end()));
            expect(back.has_value()).toBeTruthy();
            if (back) {
                Editor ed(std::move(*back));
                expect(ed.timeline().captions().size()).toBe(1U);
                expect(ed.new_caption_id().value() > id.value()).toBeTruthy();
            }
        });
    });
}
