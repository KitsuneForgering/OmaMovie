// What a timewarp producer imports as (mlt_timewarp.hpp).
#include "mlt_timewarp.hpp"

#include "oma_test.hpp"

namespace {

// Four timewarp frames from frame 2 at `speed` are `start` and `length` source seconds.
bool reads_speed(const char* speed, oma::Rational exact, double start, double length) {
    const auto r = oma::importers::import_mlt(warped(speed));
    const oma::ir::Clip* c = only_clip(r);
    return c && c->speed == exact && r->timeline.media.size() == 1 &&
           r->timeline.media[0].target == "/m/a.mp4" &&
           c->source.start().seconds_approx() == start &&
           c->source.duration().seconds_approx() == length;
}

} // namespace

void run_mlt_timewarp_tests() {
    describe("MLT timewarp import", {
        it("reads a speed change exactly, from the file it warps", {
            expect(reads_speed("2", oma::Rational::literal(2, 1), 4.0 / 30, 8.0 / 30)).toBeTruthy();
            expect(reads_speed("0.5", oma::Rational::literal(1, 2), 1.0 / 30, 2.0 / 30))
                .toBeTruthy();
            expect(reads_speed("1.25", oma::Rational::literal(5, 4), 2.5 / 30, 5.0 / 30))
                .toBeTruthy();
        });
    });
}
