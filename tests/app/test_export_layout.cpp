#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <cstdlib>

#include "oma_test.hpp"

namespace {
constexpr auto kCamera = "tests/fixtures/generated/h264_30fps_aac.mp4";
constexpr int kW = 320;
constexpr int kH = 180;

// The Y codes of frame `n` of `file` (320x180) as stored, decoded by ffmpeg.
QByteArray gray_frame(const QString& file, int n) {
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", {"-v", "error", "-i", file, "-vf",
                            QStringLiteral("select=eq(n\\,%1),extractplanes=y").arg(n), "-frames:v",
                            "1", "-f", "rawvideo", "-"});
    const bool ok = ffmpeg.waitForFinished(20000) && ffmpeg.exitCode() == 0;
    const QByteArray out = ok ? ffmpeg.readAllStandardOutput() : QByteArray();
    return out.size() == kW * kH ? out : QByteArray();
}

// Mean absolute difference between columns [a, a + width) of `x` and [b, b + width) of `y`.
double columns_diff(const QByteArray& x, int a, const QByteArray& y, int b, int width) {
    long sum = 0;
    for (int row = 0; row < kH; ++row)
        for (int i = 0; i < width; ++i)
            sum += std::abs(static_cast<unsigned char>(x[(row * kW) + a + i]) -
                            static_cast<unsigned char>(y[(row * kW) + b + i]));
    return static_cast<double>(sum) / (kH * width);
}

// The camera alone, or connected above itself at 0.2 s and laid out as a split screen, exported.
QString export_camera(const QTemporaryDir& dir, bool split) {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo(kCamera).absoluteFilePath());
    if (!dir.isValid() || !wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return {};
    if (split) {
        s.selectMedia(0);
        s.seek(0.2);
        s.connectSelected();
        s.applyLayout(1);
    }
    const QString movie = dir.filePath(split ? "split.mp4" : "plain.mp4");
    s.setExportEncoder("software");
    s.exportMovie(QUrl::fromLocalFile(movie), "mp4-h264");
    const bool done = wait_for([&] { return s.exportProgress() < 0; }, 60000);
    return done && s.exportedFile() == movie ? movie : QString();
}

// Against the camera exported alone (same matrix and transfer, so only the layout differs):
// frame 3 is untouched, and frame 15 shows the middle half of frame 15 on the left and of the
// overlay's source frame 9 on the right (independent decode).
bool exports_both_halves() {
    QTemporaryDir dir;
    const QString plain = export_camera(dir, false);
    const QString movie = export_camera(dir, true);
    const QByteArray before = gray_frame(movie, 3);
    const QByteArray split = gray_frame(movie, 15);
    const QByteArray p3 = gray_frame(plain, 3);
    const QByteArray p9 = gray_frame(plain, 9);
    const QByteArray p15 = gray_frame(plain, 15);
    if (before.isEmpty() || split.isEmpty() || p3.isEmpty() || p9.isEmpty() || p15.isEmpty())
        return false;
    constexpr double kEncode = 3.0; // two lossy encodes of the same composite
    const double left = columns_diff(split, 0, p15, 80, 160);
    const double right = columns_diff(split, 160, p9, 80, 160);
    // Not the unframed picture: the same columns of the plain export differ far more.
    const double unframed = columns_diff(split, 0, p15, 0, 160);
    return columns_diff(before, 0, p3, 0, kW) < kEncode && left < kEncode && right < kEncode &&
           unframed > 4 * kEncode;
}
} // namespace

void run_export_layout_tests() {
    describe("Exported split screen", {
        it("shows the middle half of the storyline and of the overlay side by side",
           { expect(exports_both_halves()).toBeTruthy(); });
    });
}
