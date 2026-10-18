// M10 audio boundaries, VFR: the clicks of pulse_vfr.mov sit beside H.264 video with every third
// frame dropped (frame gaps of 1/30 and 1/15 s). MP4's audio clock is the sample rate, so the
// clicks must land on their exact samples, for any block partition: the video's irregular frame
// times play no part in where audio goes (2026-10-15).
#include "pulse_clicks.hpp"

#include "oma_test.hpp"

using namespace playback_test;

namespace {

bool clicks_land_on_their_samples() {
    const std::vector<std::int64_t> want{2000, 38'000}; // 1000 + (24000 | 60000 - 23000)
    return clicks(1024, "pulse_vfr.mov", 0, true) == want &&
           clicks(1601, "pulse_vfr.mov", 0, true) == want;
}

} // namespace

void run_vfr_audio_tests() {
    describe("TimelineAudio beside VFR video", {
        it("places clicks on their exact samples", {
            if (have_fixture("pulse_vfr.mov"))
                expect(clicks_land_on_their_samples()).toBeTruthy();
        });
    });
}
