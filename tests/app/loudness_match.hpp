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

// The source frame heard in exported frame `k` over samples [at, at + length) of it (by default
// its middle half; at 2x one source frame is the first half), or -1 for silence.
inline int heard_frame(const std::vector<float>& samples, int k, int at = 400, int length = 800) {
    const auto from = static_cast<std::size_t>(k) * 1600 + static_cast<std::size_t>(at);
    const auto to = from + static_cast<std::size_t>(length);
    if (samples.size() < to)
        return -2;
    double sum = 0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double>(samples[i]) * samples[i];
    return static_cast<int>(std::lround(std::sqrt(sum / length) * std::sqrt(2.0) / 0.9 * 31 - 1));
}
