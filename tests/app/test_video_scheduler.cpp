// VideoScheduler's frame accounting: every frame a clock jumping ahead passes is shown late,
// dropped (prepared, never shown) or skipped (never prepared), and the producer reports frames it
// queued already due as late.
#include "../playback/playback_test.hpp"
#include "video_scheduler.hpp"
#include "wait_for.hpp"

#include <memory>

#include "oma_test.hpp"

namespace {
namespace pt = playback_test;

// The camera's 30 frames from frame 0; the clock shows frame 0, then jumps to frame 20. The queue
// holds kAhead (8) frames, so the producer cannot have passed frame 9 when the clock jumps.
bool accounts_for_every_frame_the_clock_passes() {
    pt::Sequence seq = pt::make_sequence();
    if (!pt::place_camera(seq, 0).valid())
        return false;
    VideoScheduler scheduler;
    scheduler.start(std::make_shared<const oma::timeline::Timeline>(seq.editor.timeline()),
                    std::make_shared<const MediaPaths>(pt::paths()),
                    std::make_shared<const LutTables>(), 320, 180, 1600, 0, 1, 29);
    std::int64_t shown = -1;
    std::int64_t passed_shown = 0; // frames between 0 and 20 that a take still returned
    const auto show = [&](std::int64_t target) {
        if (auto f = scheduler.take(target)) {
            shown = f->frame;
            passed_shown += shown > 0 && shown < 20 ? 1 : 0;
        }
        return shown == target;
    };
    if (!wait_for([&] { return show(0); }) || !wait_for([&] { return show(20); }))
        return false;
    scheduler.stop();
    // Frames 1-19: each shown late, dropped or skipped, once. (The frame being prepared when the
    // clock jumps is queued after it and shown by the next take.)
    return passed_shown + scheduler.dropped() + scheduler.skipped() == 19 &&
           scheduler.skipped() >= 10 && scheduler.late() >= 2; // 0 and 20 were due when queued
}
} // namespace

void run_video_scheduler_tests() {
    describe("VideoScheduler frame accounting", {
        it("counts every frame a jumping clock passes as dropped or skipped",
           { expect(accounts_for_every_frame_the_clock_passes()).toBeTruthy(); });
    });
}
