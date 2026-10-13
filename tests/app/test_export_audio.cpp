#include "slip_export.hpp"

#include <QFileInfo>
#include <QProcess>

#include <cmath>
#include <cstring>
#include <vector>

#include "oma_test.hpp"

namespace {
// The source frame whose loudness step is heard in the middle of exported frame `k`
// (h264_30fps_steps.mkv plays 0.9·(k+1)/31 of full scale during frame k), decoded by ffmpeg.
int heard_frame(const std::vector<float>& samples, int k) {
    double sum = 0;
    for (int i = k * 1600 + 400; i < k * 1600 + 1200; ++i)
        sum += static_cast<double>(samples[static_cast<std::size_t>(i)]) *
               samples[static_cast<std::size_t>(i)];
    return static_cast<int>(std::lround(std::sqrt(sum / 800) * std::sqrt(2.0) / 0.9 * 31 - 1));
}

// Split at frame 15 and slip the second clip three frames earlier: its own sound must follow
// the picture, so frames 15.. play the steps of source frames 12.. Only the first channel is read:
// a downmix of the stereo export would add the two copies of the mono source.
bool exported_sound_follows_the_slip() {
    QTemporaryDir dir;
    const QString movie = export_slipped(dir, "tests/fixtures/generated/h264_30fps_steps.mkv");
    if (movie.isEmpty())
        return false;
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", {"-v", "error", "-i", movie, "-af", "pan=mono|c0=c0", "-ar", "48000",
                            "-f", "f32le", "-"});
    if (!ffmpeg.waitForFinished(20000) || ffmpeg.exitCode() != 0)
        return false;
    const QByteArray raw = ffmpeg.readAllStandardOutput();
    std::vector<float> samples(static_cast<std::size_t>(raw.size()) / sizeof(float));
    std::memcpy(samples.data(), raw.constData(), samples.size() * sizeof(float));
    return samples.size() >= 30 * 1600 && heard_frame(samples, 0) == 0 &&
           heard_frame(samples, 14) == 14 && heard_frame(samples, 15) == 12 &&
           heard_frame(samples, 29) == 26;
}
} // namespace

void run_export_audio_tests() {
    describe("Exported sound", {
        it("keeps a slipped clip's sound with its picture in an independently decoded movie",
           { expect(exported_sound_follows_the_slip()).toBeTruthy(); });
    });
}
