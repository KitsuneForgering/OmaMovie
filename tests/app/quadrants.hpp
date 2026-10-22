#pragma once

// Quadrants of 320x180 gray frames decoded by ffmpeg, for checking scaled layers against the
// same source frame decoded and halved by ffmpeg (an independent scaler).
#include <QByteArray>
#include <QProcess>
#include <QString>

#include <cstdlib>

inline constexpr int kQuadW = 160;
inline constexpr int kQuadH = 90;

// Frame `n` of a 320x180 `file` as gray, at full size or halved.
inline QByteArray gray_at(const QString& file, int n, bool half) {
    const int w = half ? kQuadW : 2 * kQuadW;
    const int h = half ? kQuadH : 2 * kQuadH;
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg",
                 {"-v", "error", "-i", file, "-vf",
                  QStringLiteral("select=eq(n\\,%1),scale=%2:%3,format=gray").arg(n).arg(w).arg(h),
                  "-frames:v", "1", "-f", "rawvideo", "-"});
    const bool ok = ffmpeg.waitForFinished(20000) && ffmpeg.exitCode() == 0;
    const QByteArray out = ok ? ffmpeg.readAllStandardOutput() : QByteArray();
    return out.size() == static_cast<qsizetype>(w) * h ? out : QByteArray();
}

// Mean absolute difference between quadrant (qx, qy) of full frame `out` and `ref`: the same
// quadrant of a full frame, or a whole halved frame. A 4-pixel border is skipped, where scalers
// and chroma siting differ most.
inline double quadrant_diff(const QByteArray& out, int qx, int qy, const QByteArray& ref) {
    const bool half = ref.size() == static_cast<qsizetype>(kQuadW) * kQuadH;
    long sum = 0;
    long n = 0;
    for (int y = 4; y < kQuadH - 4; ++y)
        for (int x = 4; x < kQuadW - 4; ++x) {
            const int at = (((qy * kQuadH) + y) * 2 * kQuadW) + (qx * kQuadW) + x;
            const int r = half ? (y * kQuadW) + x : at;
            sum +=
                std::abs(static_cast<unsigned char>(out[at]) - static_cast<unsigned char>(ref[r]));
            ++n;
        }
    return static_cast<double>(sum) / static_cast<double>(n);
}
