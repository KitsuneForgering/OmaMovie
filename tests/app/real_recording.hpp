#pragma once

// Picture checks for a real Omarchy recording (OMA_REAL_RECORDING, M10): its content is arbitrary
// and a screen repeats frames, so a frame matches when no other frame nearby matches clearly
// better. Sound: sound_sync.hpp.
#include "frame_match.hpp"

#include <algorithm>
#include <cmath>

inline long frame_sad(const QByteArray& a, int i, const QByteArray& b, int j) {
    long sad = 0;
    for (qsizetype p = 0; p < kFrame; ++p)
        sad += std::abs(static_cast<unsigned char>(a[i * kFrame + p]) -
                        static_cast<unsigned char>(b[j * kFrame + p]));
    return sad;
}

// Exported frame `i` shows source frame `j`: within 2 levels per pixel of it (an export measures
// ~0.2; a level shift such as the old export transfer, ~12), and no source frame among j ± 30
// matches better by more than 1/8 of a level per pixel. A wrong frame where something moves
// matches thousands of units worse; on a still screen the repeats of `j` match within noise.
inline bool shows_frame(const QByteArray& source, const QByteArray& out, int i, int j) {
    const int count = static_cast<int>(source.size() / kFrame);
    const long expected = frame_sad(source, j, out, i);
    if (expected > kFrame * 2)
        return false;
    for (int s = std::max(0, j - 30); s < std::min(count, j + 31); ++s)
        if (expected - frame_sad(source, s, out, i) > kFrame / 8)
            return false;
    return true;
}
