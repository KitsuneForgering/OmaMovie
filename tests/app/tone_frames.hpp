#pragma once

// Sound identity for screenrecording_like.mp4 (tests/fixtures/generate.sh): frame k plays a
// 120·(k+4) Hz tone, so a decoded window names its source frame by frequency, which survives the
// codec's gain errors where loudness steps did not.
#include <cmath>
#include <cstddef>
#include <vector>

// The source frame heard in exported 60 fps frame `k`: the strongest of the fixture's tones,
// 120·(n+4) Hz, over the middle half of the frame (400 samples at 48 kHz, whole cycles of each
// tone, so the bins are orthogonal). -1 for silence, -2 past the end.
inline int heard_recording_frame(const std::vector<float>& samples, int k) {
    const auto from = static_cast<std::size_t>(k) * 800 + 200;
    if (samples.size() < from + 400)
        return -2;
    int best = -1;
    double best_power = 1e-3; // below this the window is silence
    for (int n = 0; n < 66; ++n) {
        double re = 0;
        double im = 0;
        for (std::size_t i = 0; i < 400; ++i) {
            const double phase = 2 * M_PI * (n + 4) * static_cast<double>(i) / 400;
            re += samples[from + i] * std::cos(phase);
            im += samples[from + i] * std::sin(phase);
        }
        if (const double power = ((re * re) + (im * im)) / (400.0 * 400.0); power > best_power)
            best = n, best_power = power;
    }
    return best;
}
