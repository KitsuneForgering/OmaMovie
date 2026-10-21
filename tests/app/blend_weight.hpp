#pragma once

// The weight of a mix in gray thumbnails (frame_match.hpp). OmaMovie mixes in linear light
// (ADR-0006) and converts an untagged SD source's colour, so values are linearised with a 2.4
// power and mapped by a gain and offset fitted on frames outside the mix before fitting the
// mix itself; on coded values a correct dissolve read 0.12 where it starts.
#include "frame_match.hpp"

#include <cmath>
#include <utility>
#include <vector>

struct GrayFrames {
    const QByteArray& bytes;
    double at(int frame, qsizetype p) const {
        return std::pow(static_cast<unsigned char>(bytes[frame * kFrame + p]) / 255.0, 2.4);
    }
};

// Least-squares gain and offset from source frame `.second` to output frame `.first`.
inline std::pair<double, double> level_map(GrayFrames source, GrayFrames out,
                                           const std::vector<std::pair<int, int>>& pairs) {
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (const auto& [o, s] : pairs)
        for (qsizetype p = 0; p < kFrame; ++p) {
            const double x = source.at(s, p);
            const double y = out.at(o, p);
            n += 1;
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
        }
    const double gain = ((n * sxy) - (sx * sy)) / ((n * sxx) - (sx * sx));
    return {gain, (sy - (gain * sx)) / n};
}

// Least-squares t in out[i] ≈ (1 − t)·source[a] + t·source[b], both through `map`.
inline double blend_weight(GrayFrames source, GrayFrames out, std::pair<double, double> map, int a,
                           int b, int i) {
    double num = 0.0;
    double den = 0.0;
    for (qsizetype p = 0; p < kFrame; ++p) {
        const double va = (map.first * source.at(a, p)) + map.second;
        const double d = map.first * (source.at(b, p) - source.at(a, p));
        num += (out.at(i, p) - va) * d;
        den += d * d;
    }
    return den > 0.0 ? num / den : -1.0;
}
