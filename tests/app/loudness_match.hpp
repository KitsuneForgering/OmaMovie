#pragma once

// h264_30fps_steps.mkv plays 0.9·(k+1)/31 of full scale during frame k: the loudness of a decoded
// window names its source frame. Decoded by ffmpeg, first channel only (a downmix of a stereo
// export would add the two copies of the mono source).
#include <QByteArray>
#include <QProcess>
#include <QString>

#include <cmath>
#include <cstring>
#include <vector>

inline std::vector<float> first_channel(const QString& movie) {
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", {"-v", "error", "-i", movie, "-af", "pan=mono|c0=c0", "-ar", "48000",
                            "-f", "f32le", "-"});
    if (!ffmpeg.waitForFinished(20000) || ffmpeg.exitCode() != 0)
        return {};
    const QByteArray raw = ffmpeg.readAllStandardOutput();
    std::vector<float> samples(static_cast<std::size_t>(raw.size()) / sizeof(float));
    std::memcpy(samples.data(), raw.constData(), samples.size() * sizeof(float));
    return samples;
}

// The source frame heard in the middle of exported frame `k` (-1 for silence).
inline int heard_frame(const std::vector<float>& samples, int k) {
    if (samples.size() < static_cast<std::size_t>(k + 1) * 1600)
        return -2;
    double sum = 0;
    for (std::size_t i = static_cast<std::size_t>(k) * 1600 + 400;
         i < static_cast<std::size_t>(k) * 1600 + 1200; ++i)
        sum += static_cast<double>(samples[i]) * samples[i];
    return static_cast<int>(std::lround(std::sqrt(sum / 800) * std::sqrt(2.0) / 0.9 * 31 - 1));
}
