#include "edit_export.hpp"
#include "loudness_match.hpp"

#include "oma_test.hpp"

namespace {
constexpr auto kSteps = "tests/fixtures/generated/h264_30fps_steps.mkv";

// The second clip slipped three frames earlier: its own sound follows the picture, so frames
// 15.. play the steps of source frames 12..
bool exported_sound_follows_the_slip() {
    QTemporaryDir dir;
    const auto samples = first_channel(export_slipped(dir, kSteps));
    return heard_frame(samples, 0) == 0 && heard_frame(samples, 14) == 14 &&
           heard_frame(samples, 15) == 12 && heard_frame(samples, 29) == 26;
}

// The second clip's sound detached onto a lane, then the clip slid two frames earlier: the
// connected sound moves with it, so frame 13 plays source frame 15 instead of silence.
bool detached_sound_follows_a_slide() {
    QTemporaryDir dir;
    const auto samples = first_channel(export_edit(dir, kSteps, [](Session& s) {
        s.detachAudio();
        s.selectClip(s.clips().at(1).toMap().value("id").toDouble()); // detach selects the sound
        s.nudgeSlide(-2);
    }));
    return heard_frame(samples, 12) == 12 && heard_frame(samples, 13) == 15 &&
           heard_frame(samples, 26) == 28;
}
} // namespace

void run_export_audio_tests() {
    describe("Exported sound", {
        it("keeps a slipped clip's sound with its picture in an independently decoded movie",
           { expect(exported_sound_follows_the_slip()).toBeTruthy(); });
        it("moves a detached sound with its slid clip",
           { expect(detached_sound_follows_a_slide()).toBeTruthy(); });
    });
}
