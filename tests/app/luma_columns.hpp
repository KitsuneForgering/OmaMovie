#pragma once

#include <QByteArray>
#include <QProcess>
#include <QString>

#include <vector>

// The mean Y code of each column of the first frame (320x180), as stored in the file.
inline std::vector<double> luma_columns(const QString& file) {
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", {"-v", "error", "-i", file, "-vf", "extractplanes=y", "-frames:v", "1",
                            "-f", "rawvideo", "-"});
    if (!ffmpeg.waitForFinished(20000) || ffmpeg.exitCode() != 0)
        return {};
    const QByteArray y = ffmpeg.readAllStandardOutput();
    if (y.size() != 320 * 180)
        return {};
    std::vector<double> columns(320, 0.0);
    for (int row = 0; row < 180; ++row)
        for (int x = 0; x < 320; ++x)
            columns[x] += static_cast<unsigned char>(y[(row * 320) + x]) / 180.0;
    return columns;
}
