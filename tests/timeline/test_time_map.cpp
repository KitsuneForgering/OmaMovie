#include "fixture.hpp"

#include "oma/timeline/evaluate.hpp"

#include <vector>

#include "oma_test.hpp"

using namespace timeline_test;

namespace {

using Kind = oma::timeline::TimeSegment::Kind;

// One tick per frame at 30 fps keeps the hand-computed offsets readable.
const oma::Rational kTb = q(1, 30);

oma::timeline::TimeSegment seg(Kind kind, std::int64_t length, oma::Rational from = q(1, 1),
                               oma::Rational to = q(1, 1)) {
    return {.kind = kind, .length = length, .from = from, .to = to};
}

oma::RationalTime secs(std::int64_t num, std::int64_t den) {
    return oma::RationalTime::make(num, q(1, den)).value();
}

bool offset_is(const TimeMap& m, std::int64_t ticks, const oma::RationalTime& expected) {
    auto o = m.media_offset(ticks, kTb);
    return o.has_value() && *o == expected;
}

// Forward for a second, hold half a second, then back to the start (a boomerang).
bool linear_freeze_reverse() {
    auto m = TimeMap::segmented(
        {seg(Kind::Linear, 30), seg(Kind::Freeze, 15), seg(Kind::Linear, 30, q(-1, 1))});
    if (!m) {
        return false;
    }
    auto span = m->extent(75, kTb);
    return m->length() == 75 && offset_is(*m, 30, secs(1, 1)) && offset_is(*m, 40, secs(1, 1)) &&
           offset_is(*m, 60, secs(1, 2)) && offset_is(*m, 75, secs(0, 1)) && span.has_value() &&
           span->first == secs(0, 1) && span->second == secs(1, 1);
}

// Speed 1 -> 3 over 30 ticks: from*t + (to - from)*t^2/(2L) ticks of media.
// t = 15: 15 + 2*225/60 = 22.5 ticks = 3/4 s; t = 30: 30 + 30 = 60 ticks = 2 s.
bool ramp_integrates_speed() {
    auto m = TimeMap::segmented({seg(Kind::Ramp, 30, q(1, 1), q(3, 1))});
    return m.has_value() && offset_is(*m, 15, secs(3, 4)) && offset_is(*m, 30, secs(2, 1));
}

// The second half of that ramp is a ramp from 2 to 3 that moves exactly what the original
// moved there (37.5 ticks = 5/4 s).
bool slice_keeps_motion() {
    auto m = TimeMap::segmented(
        {seg(Kind::Linear, 10), seg(Kind::Ramp, 30, q(1, 1), q(3, 1)), seg(Kind::Freeze, 5)});
    if (!m) {
        return false;
    }
    auto half = m->slice(25, 40);
    auto tail = m->slice(25, 45);
    if (!half || !tail || half->segments().size() != 1 || tail->segments().size() != 2) {
        return false;
    }
    const auto& r = half->segments()[0];
    auto whole_from = m->media_offset(25, kTb);
    auto whole_to = m->media_offset(40, kTb);
    auto part = half->media_offset(15, kTb);
    return r.kind == Kind::Ramp && r.from == q(2, 1) && r.to == q(3, 1) && r.length == 15 &&
           part.has_value() && *part == secs(5, 4) && whole_from && whole_to &&
           tail->segments()[1].kind == Kind::Freeze && tail->length() == 20;
}

bool rejects_invalid_segments() {
    return !TimeMap::segmented({}).has_value() &&
           !TimeMap::segmented({seg(Kind::Linear, 0)}).has_value() &&
           !TimeMap::segmented({seg(Kind::Linear, 10, q(0, 1))}).has_value() &&
           !TimeMap::segmented({seg(Kind::Ramp, 10, q(1, 1), q(-1, 1))}).has_value() &&
           !TimeMap::segmented({seg(Kind::Ramp, 10, q(0, 1), q(0, 1))}).has_value() &&
           TimeMap::segmented({seg(Kind::Ramp, 10, q(0, 1), q(-2, 1))}).has_value();
}

// A ramp too long for 64-bit results reports overflow instead of wrapping.
bool ramp_overflow_is_an_error() {
    const std::int64_t huge = std::int64_t{1} << 62;
    auto m = TimeMap::segmented({seg(Kind::Ramp, huge, q(1, 3), q(7, 5))});
    if (!m) {
        return false;
    }
    auto o = m->media_offset(huge - 1, kTb);
    return !o.has_value() && o.error().code() == oma::ErrorCode::Overflow;
}

// The media frame (camera frames are 3000 ticks of 1/90000) shown at sequence frame `at`.
std::int64_t frame_at(const Timeline& tl, std::int64_t at) {
    auto c = evaluate(tl, f(at));
    if (!c || c->video.empty()) {
        return -1;
    }
    return c->video.back().media_time.value() / 3000;
}

// Frames 10..29 played backwards show 29 down to 10, never 30, and undo restores the clip.
bool reverse_shows_the_same_frames_backwards() {
    auto fx = make_fixture();
    const ClipId id = place(fx, fx.video, 0, 10, 20);
    if (!fx.editor.execute(edit::reverse(id))) {
        return false;
    }
    const Timeline& tl = fx.editor.timeline();
    const bool frames = frame_at(tl, 0) == 29 && frame_at(tl, 1) == 28 && frame_at(tl, 19) == 10;
    const bool valid = tl.validate().has_value() && tl.find_clip(id)->duration.value() == 20;
    return frames && valid && fx.editor.undo().has_value() &&
           fx.editor.timeline().find_clip(id)->time_map.is_constant() &&
           frame_at(fx.editor.timeline(), 0) == 10;
}

// A 5-frame freeze at frame 4 of a 10-frame clip: frames 0..3, frame 4 held for 5, then 4..9.
bool freeze_holds_then_continues() {
    auto fx = make_fixture();
    const ClipId id = place(fx, fx.video, 0, 0, 10);
    if (!fx.editor.execute(edit::freeze_frame(id, f(4), f(5), true))) {
        return false;
    }
    const Timeline& tl = fx.editor.timeline();
    return tl.find_clip(id)->duration.value() == 15 && frame_at(tl, 3) == 3 &&
           frame_at(tl, 4) == 4 && frame_at(tl, 8) == 4 && frame_at(tl, 9) == 4 &&
           frame_at(tl, 10) == 5 && frame_at(tl, 14) == 9 && tl.validate().has_value();
}

std::vector<std::int64_t> frames_of(const Timeline& tl, int count) {
    std::vector<std::int64_t> out;
    for (int i = 0; i < count; ++i) {
        out.push_back(frame_at(tl, i));
    }
    return out;
}

// Splitting and trimming a frozen clip keep every frame where it was.
bool split_and_trim_keep_segmented_motion() {
    auto fx = make_fixture();
    const ClipId id = place(fx, fx.video, 0, 0, 10);
    if (!fx.editor.execute(edit::freeze_frame(id, f(4), f(5), true))) {
        return false;
    }
    const auto before = frames_of(fx.editor.timeline(), 15);
    if (!fx.editor.execute(edit::split(id, f(6)))) {
        return false;
    }
    const auto after = frames_of(fx.editor.timeline(), 15);
    // Trimming the head's end back into the freeze and out again extends the freeze, which
    // restores it exactly (an outward trim continues the edge segment, ADR-0013).
    const bool trims = fx.editor.execute(edit::trim_end(id, f(5), false)).has_value() &&
                       fx.editor.execute(edit::trim_end(id, f(6), false)).has_value() &&
                       frames_of(fx.editor.timeline(), 15) == before;
    return before == after && trims && fx.editor.timeline().validate().has_value();
}

// A reverse map starting at the first frame would need media before the file: rejected.
bool reverse_beyond_media_is_rejected() {
    auto fx = make_fixture();
    const ClipId id = place(fx, fx.video, 0, 0, 10);
    auto map = TimeMap::segmented({seg(Kind::Linear, 10, q(-1, 1))});
    return map.has_value() && !fx.editor.execute(edit::set_time_map(id, *map, false)).has_value() &&
           fx.editor.timeline().find_clip(id)->time_map.is_constant();
}

} // namespace

void run_time_map_tests() {
    describe("timeline::TimeMap segments (ADR-0013)", {
        it("plays forward, holds and plays back exactly",
           { expect(linear_freeze_reverse()).toBeTruthy(); });
        it("integrates a linear speed ramp exactly",
           { expect(ramp_integrates_speed()).toBeTruthy(); });
        it("slices segments without changing their motion",
           { expect(slice_keeps_motion()).toBeTruthy(); });
        it("rejects segments that are empty, stopped or change direction",
           { expect(rejects_invalid_segments()).toBeTruthy(); });
        it("reverses a clip over the same frames",
           { expect(reverse_shows_the_same_frames_backwards()).toBeTruthy(); });
        it("holds a freeze frame and continues after it",
           { expect(freeze_holds_then_continues()).toBeTruthy(); });
        it("keeps segmented motion through split and trims",
           { expect(split_and_trim_keep_segmented_motion()).toBeTruthy(); });
        it("rejects a time map that leaves the media",
           { expect(reverse_beyond_media_is_rejected()).toBeTruthy(); });
        it("reports overflow on ramps too long to evaluate",
           { expect(ramp_overflow_is_an_error()).toBeTruthy(); });
    });
}
