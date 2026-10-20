// A speed change as Kdenlive writes it (an MLT timewarp producer), against melt's own render of
// melt_timewarp_23976.mlt: double speed, half speed and normal speed in one playlist.
#include "shared_scene.hpp"

#include "oma_test.hpp"

namespace {
constexpr auto kSource = "tests/fixtures/generated/h264_23.976fps.mp4";

// melt shows source 2-10 by twos, 4 5 5 6 6 7 at half speed and 16-21. Half speed lands on
// source half frames (4.5, 5.5, 6.5): melt rounds them up, OmaMovie shows the frame already on
// screen, so each source frame plays twice from the clip's in point (4 4 5 5 6 6).
Scene timewarp() {
    std::vector<int> expected{2, 4, 6, 8, 10, 4, 4, 5, 5, 6, 6};
    for (int f = 16; f <= 21; ++f)
        expected.push_back(f);
    return {"melt_timewarp_23976.mlt", 3, kSource, 24, expected};
}
} // namespace

void run_shared_speed_tests() {
    describe("Shared cross-editor speed", {
        it("plays a Kdenlive/melt timewarp as melt does, except on half frames",
           { expect(exports_scene(timewarp())).toBeTruthy(); });
    });
}
