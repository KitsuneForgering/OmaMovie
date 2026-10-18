#pragma once

// Frame identity by an independent decoder: ffmpeg scales both files to small gray frames and
// each exported frame is matched to the most similar source frame.
#include <QByteArray>
#include <QProcess>
#include <QString>

#include <cstdlib>

inline constexpr qsizetype kFrame = 80 * 45; // gray thumbnails decoded by ffmpeg, not by OmaMovie

inline QByteArray gray_frames(const QString& file) {
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", {"-v", "error", "-i", file, "-vf", "scale=80:45,format=gray", "-f",
                            "rawvideo", "-"});
    return ffmpeg.waitForFinished(20000) && ffmpeg.exitCode() == 0 ? ffmpeg.readAllStandardOutput()
                                                                   : QByteArray();
}

// The source frame most like exported frame `i` (sum of absolute differences).
inline int closest(const QByteArray& source, const QByteArray& out, int i) {
    int best = -1;
    long best_sad = 0;
    for (int s = 0; s < source.size() / kFrame; ++s) {
        long sad = 0;
        for (qsizetype p = 0; p < kFrame; ++p)
            sad += std::abs(static_cast<unsigned char>(source[s * kFrame + p]) -
                            static_cast<unsigned char>(out[i * kFrame + p]));
        if (best < 0 || sad < best_sad)
            best = s, best_sad = sad;
    }
    return best;
}
