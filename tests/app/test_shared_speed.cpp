// A speed change as Kdenlive writes it (an MLT timewarp producer), against melt's own render of
// melt_timewarp_23976.mlt: double speed, half speed and normal speed in one playlist; and the
// sound of a double-speed clip from melt_timewarp_steps.mlt.
#include "shared_scene.hpp"

#include "oma_test.hpp"

namespace {
constexpr auto kSource = "tests/fixtures/generated/h264_23.976fps.mp4";
constexpr auto kSteps = "tests/fixtures/generated/h264_30fps_steps.mkv";

// melt shows source 2-10 by twos, 4 5 5 6 6 7 at half speed and 16-21. Half speed lands on
// source half frames (4.5, 5.5, 6.5): melt rounds them up, OmaMovie shows the frame already on
// screen, so each source frame plays twice from the clip's in point (4 4 5 5 6 6).
Scene timewarp() {
    std::vector<int> expected{2, 4, 6, 8, 10, 4, 4, 5, 5, 6, 6};
    for (int f = 16; f <= 21; ++f)
        expected.push_back(f);
    return {"melt_timewarp_23976.mlt", 3, kSource, 24, expected};
}

// Source 0-18 by twos, then 20-29, as melt renders it. Each frame's sound is read over samples
// 100-700, inside the first half where a 2x frame plays source 2k (11 cycles of the tone at
// 2x); melt's own render hears the same frames there, 3 dB quieter.
bool timewarp_sound_follows_picture() {
    std::vector<int> expected;
    for (int f = 0; f <= 18; f += 2)
        expected.push_back(f);
    for (int f = 20; f <= 29; ++f)
        expected.push_back(f);
    QTemporaryDir dir;
    const QString movie =
        export_reopened_import(dir, "tests/importers/fixtures/melt_timewarp_steps.mlt", 2);
    const QByteArray picture = gray_frames(QFileInfo(kSteps).absoluteFilePath());
    const QByteArray out = movie.isEmpty() ? QByteArray() : gray_frames(movie);
    const std::vector<float> samples =
        movie.isEmpty() ? std::vector<float>() : first_channel(movie);
    if (picture.size() != 30 * kFrame || out.size() != std::ssize(expected) * kFrame)
        return false;
    for (int k = 0; k < std::ssize(expected); ++k)
        if (closest(picture, out, k) != expected[static_cast<std::size_t>(k)] ||
            heard_frame(samples, k, 100, 600) != expected[static_cast<std::size_t>(k)])
            return false;
    return true;
}
} // namespace

void run_shared_speed_tests() {
    describe("Shared cross-editor speed", {
        it("plays a Kdenlive/melt timewarp as melt does, except on half frames",
           { expect(exports_scene(timewarp())).toBeTruthy(); });
        it("keeps a Kdenlive/melt double-speed clip's sound with its picture",
           { expect(timewarp_sound_follows_picture()).toBeTruthy(); });
    });
}
