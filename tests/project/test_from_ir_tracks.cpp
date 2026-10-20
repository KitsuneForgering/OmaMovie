// ProjectIR state beyond clip ranges: a disabled track and a constant speed (from MLT timewarp).
#include "from_ir_sample.hpp"

#include "oma_test.hpp"

namespace ir = oma::ir;
namespace tl = oma::timeline;
using namespace from_ir_sample;

namespace {

// The sample with V1 disabled: its clips stay, the track neither shows nor sounds.
bool disabled_track_is_hidden_and_muted() {
    ir::Timeline t = sample();
    t.tracks[0].off = true;
    ir::ImportReport report;
    const auto doc = oma::project::from_ir(t, resolve, report);
    const tl::Track* v1 =
        doc && doc->timeline ? doc->timeline->find_track(doc->storyline) : nullptr;
    return v1 && v1->clips.size() == 3 && v1->hidden && v1->muted;
}

// 48 source frames at 2x last 24 frames on the storyline and keep the constant map.
bool speed_sets_the_time_map() {
    ir::Timeline t = sample();
    auto& a = std::get<ir::Clip>(t.tracks[0].items[0]);
    a.speed = oma::Rational::literal(2, 1);
    ir::ImportReport report;
    const auto doc = oma::project::from_ir(t, resolve, report);
    const tl::Track* v1 =
        doc && doc->timeline ? doc->timeline->find_track(doc->storyline) : nullptr;
    return v1 && v1->clips[0].duration.seconds_approx() == 1.0 &&
           v1->clips[0].time_map.is_constant() &&
           v1->clips[0].time_map.speed() == oma::Rational::literal(2, 1) &&
           v1->clips[1].start.seconds_approx() == 1.5; // the gap still follows
}

} // namespace

void run_from_ir_track_tests() {
    describe("project::from_ir track state and speed", {
        it("imports a disabled track hidden and muted",
           { expect(disabled_track_is_hidden_and_muted()).toBeTruthy(); });
        it("places a clip at its speed with a constant time map",
           { expect(speed_sets_the_time_map()).toBeTruthy(); });
    });
}
