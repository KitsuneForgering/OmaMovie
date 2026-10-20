#include "shared_scene.hpp"

#include <QDebug>

#include "oma_test.hpp"

namespace {
constexpr auto kSource = "tests/fixtures/generated/h264_23.976fps.mp4";
constexpr auto kSteps = "tests/fixtures/generated/h264_30fps_steps.mkv";

// tools/bench/cross-editor/cuts.sh at fixture scale: five reordered cuts, the same edit in
// cuts.otio and in cuts.mlt as melt/Kdenlive write it.
Scene cuts(const char* name) {
    return {name, 5, kSource, 24, frames({{16, 4}, {2, 6}, {10, 3}, {0, 2}, {20, 4}})};
}
// The same shape on the steps clip, whose loudness names each source frame (cuts_steps.*).
Scene step_cuts(const char* name, const char* encoder = "software") {
    return {name, 5,      kSteps, 30, frames({{20, 4}, {3, 6}, {12, 3}, {0, 2}, {25, 5}}),
            true, encoder};
}
// roundtrip.sh at fixture scale, a document melt wrote: melt dissolves over output 10-13 and
// OmaMovie centres the dissolve on its cut (12-15); outside both windows they must agree.
Scene mix() {
    return {"melt_mix_23976.mlt", 2, kSource, 24, frames({{0, 10}, {-1, 6}, {18, 6}})};
}

// The bench's hardware route: VA-API where it opens (the Intel reference host), otherwise the
// software encoder with its reason (CI). Either way the frames and sound must be right and the
// route must say which one ran.
bool hardware_route_keeps_the_cuts() {
    std::string route;
    const bool ok = exports_scene(step_cuts("cuts_steps.mlt", "hardware"), &route);
    qInfo() << "shared cuts, hardware route:" << route.c_str();
    return ok && (route == "VA-API H.264 (GPU)" ||
                  route.starts_with("libx264 (software): the GPU encoder failed"));
}
} // namespace

void run_shared_cuts_tests() {
    describe("Shared cross-editor scenes", {
        it("exports the OTIO cuts frame for frame after save and reopen",
           { expect(exports_scene(cuts("cuts.otio"))).toBeTruthy(); });
        it("exports the same cuts from a melt/Kdenlive MLT project",
           { expect(exports_scene(cuts("cuts.mlt"))).toBeTruthy(); });
        it("agrees with melt outside its luma mix", { expect(exports_scene(mix())).toBeTruthy(); });
        it("keeps each cut's sound with its picture from OTIO",
           { expect(exports_scene(step_cuts("cuts_steps.otio"))).toBeTruthy(); });
        it("keeps each cut's sound with its picture from MLT",
           { expect(exports_scene(step_cuts("cuts_steps.mlt"))).toBeTruthy(); });
        it("keeps the cuts on the hardware route or its reported fallback",
           { expect(hardware_route_keeps_the_cuts()).toBeTruthy(); });
    });
}
