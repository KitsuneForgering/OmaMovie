#include "oma/project_ir/validate.hpp"

#include "oma_test.hpp"

namespace ir = oma::ir;
namespace {
oma::RationalTime t(std::int64_t value, std::int64_t rate) {
    return oma::RationalTime::make(value, oma::Rational::make(1, rate).value()).value();
}

oma::TimeRange range(std::int64_t start, std::int64_t duration, std::int64_t rate) {
    return oma::TimeRange::make(t(start, rate), t(duration, rate)).value();
}

// A 24 fps clip from frame 24 for 48 frames of a 10 s media item (in 1/1000 s), then a gap.
ir::Timeline sample() {
    ir::Timeline tl;
    tl.media.push_back({.id = "m1",
                        .target = "file:///a.mov",
                        .available = range(0, 10000, 1000),
                        .unsupported = {}});
    ir::Track track{.name = "V1", .kind = ir::TrackKind::Video, .items = {}, .unsupported = {}};
    track.items.emplace_back(
        ir::Clip{.name = "a", .media = "m1", .source = range(24, 48, 24), .unsupported = {}});
    track.items.emplace_back(ir::Gap{.duration = t(12, 24)});
    tl.tracks.push_back(std::move(track));
    return tl;
}
} // namespace

void run_validate_tests() {
    describe("project-ir validate", {
        it("accepts a clip inside its media and a gap",
           { expect(ir::validate(sample()).has_value()).toBeTruthy(); });

        it("refuses unknown or repeated media", {
            ir::Timeline unknown = sample();
            std::get<ir::Clip>(unknown.tracks[0].items[0]).media = "m2";
            expect(ir::validate(unknown).has_value()).toBeFalsy();
            ir::Timeline repeated = sample();
            repeated.media.push_back(repeated.media.front());
            expect(ir::validate(repeated).has_value()).toBeFalsy();
        });

        it("refuses a clip past its media, compared exactly across timebases", {
            ir::Timeline past = sample();
            // 10 s of media at 24 fps ends at frame 240: [200, 241) runs one frame over.
            std::get<ir::Clip>(past.tracks[0].items[0]).source = range(200, 41, 24);
            expect(ir::validate(past).has_value()).toBeFalsy();
            std::get<ir::Clip>(past.tracks[0].items[0]).source = range(200, 40, 24);
            expect(ir::validate(past).has_value()).toBeTruthy();
        });

        it("refuses negative gaps", {
            ir::Timeline gap = sample();
            gap.tracks[0].items[1] = ir::Gap{.duration = t(-1, 24)};
            expect(ir::validate(gap).has_value()).toBeFalsy();
        });
    });
}
