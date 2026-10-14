#pragma once

// Sound checks for a real recording (M10): the exported sound must correlate best with the
// recording at zero lag. Content is arbitrary, so windows that are near silence are skipped.
#include <QByteArray>
#include <QProcess>
#include <QString>

#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

// All channels mixed to one at 48 kHz, by ffmpeg: a recorder may put a mono microphone on one
// side only.
inline std::vector<float> mixed_sound(const QString& movie) {
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg",
                 {"-v", "error", "-i", movie, "-ac", "1", "-ar", "48000", "-f", "f32le", "-"});
    if (!ffmpeg.waitForFinished(120000) || ffmpeg.exitCode() != 0)
        return {};
    const QByteArray raw = ffmpeg.readAllStandardOutput();
    std::vector<float> samples(static_cast<std::size_t>(raw.size()) / sizeof(float));
    std::memcpy(samples.data(), raw.constData(), samples.size() * sizeof(float));
    return samples;
}

// The lag (±10 ms at 48 kHz) at which 0.5 s of exported sound from `at` best correlates with the
// source from `at + offset`; nothing for a window near silence or past either end.
inline std::optional<int> best_lag(const std::vector<float>& source, const std::vector<float>& out,
                                   std::size_t at, std::size_t offset) {
    constexpr std::size_t kWindow = 24000;
    constexpr int kSearch = 480;
    if (at + kWindow > out.size() || at + offset + kWindow + kSearch > source.size() ||
        at + offset < kSearch)
        return std::nullopt;
    double energy = 0;
    for (std::size_t i = 0; i < kWindow; ++i)
        energy += static_cast<double>(out[at + i]) * out[at + i];
    if (energy < 1e-3)
        return std::nullopt;
    int best = 0;
    double best_corr = -1;
    for (int lag = -kSearch; lag <= kSearch; ++lag) {
        double dot = 0;
        double norm = 0; // normalized per lag, so a louder neighbouring window cannot win
        const std::size_t from = at + offset + static_cast<std::size_t>(lag + kSearch) - kSearch;
        for (std::size_t i = 0; i < kWindow; ++i) {
            dot += static_cast<double>(out[at + i]) * source[from + i];
            norm += static_cast<double>(source[from + i]) * source[from + i];
        }
        if (const double corr = norm > 0 ? dot / std::sqrt(norm) : -1; corr > best_corr)
            best = lag, best_corr = corr;
    }
    return best;
}
