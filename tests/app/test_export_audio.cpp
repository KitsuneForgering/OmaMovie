#include "edit_export.hpp"
#include "loudness_match.hpp"

#include "oma_test.hpp"

namespace {
constexpr auto kSteps = "tests/fixtures/generated/h264_30fps_steps.mkv";

// The second clip slipped three frames earlier: its own sound follows the
// picture, so frames 15.. play the steps of source frames 12..
bool exported_sound_follows_the_slip() {
  QTemporaryDir dir;
  const auto samples = first_channel(export_slipped(dir, kSteps));
  return heard_frame(samples, 0) == 0 && heard_frame(samples, 14) == 14 &&
         heard_frame(samples, 15) == 12 && heard_frame(samples, 29) == 26;
}

} // namespace

void run_export_audio_tests() {
  describe("Exported sound", {
    it("keeps a slipped clip's sound with its picture in an independently "
       "decoded movie",
       { expect(exported_sound_follows_the_slip()).toBeTruthy(); });
  });
}
