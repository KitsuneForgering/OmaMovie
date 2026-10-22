#pragma once

// A CPU reference for VulkanCompositor::read_nv12: the same render's BT.709 RGBA8 converted with
// the BT.709 matrix, limited range and chroma sited left (H.264's default): a [1 2 1] / 4 filter
// about each even column, the edge repeated, and the mean of the two rows. The GPU converts from
// float, this from 8 bits: they may differ by a level or two, not more.

#include "oma/compositor/compositor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace nv12_reference {

inline double luma(const std::uint8_t* p) {
    return 16.0 + (219.0 * (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) / 255.0);
}

// The largest luma and chroma differences, in levels.
inline std::array<int, 2> nv12_vs_rgba(const oma::compositor::VulkanCompositor::Nv12Image& nv12,
                                       const std::vector<std::uint8_t>& rgba) {
    std::array<int, 2> worst{0, 0};
    const std::uint32_t w = nv12.width;
    for (std::uint32_t y = 0; y < nv12.height; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const int want = static_cast<int>(std::lround(luma(&rgba[((y * w) + x) * 4])));
            worst[0] = std::max(worst[0], std::abs(nv12.bytes[(y * nv12.stride) + x] - want));
        }
    const std::size_t uv = static_cast<std::size_t>(nv12.stride) * nv12.height;
    for (std::uint32_t y = 0; y < nv12.height / 2; ++y)
        for (std::uint32_t x = 0; x < w / 2; ++x) {
            std::array<double, 3> m{0, 0, 0};
            const std::array<std::uint32_t, 3> columns{(2 * x) == 0 ? 0 : (2 * x) - 1, 2 * x,
                                                       std::min((2 * x) + 1, w - 1)};
            const std::array<double, 3> weights{0.25, 0.5, 0.25};
            for (std::uint32_t r = 0; r < 2; ++r)
                for (std::size_t k = 0; k < 3; ++k)
                    for (std::size_t c = 0; c < 3; ++c)
                        m[c] +=
                            weights[k] * rgba[((((2 * y) + r) * w) + columns[k]) * 4 + c] / 510.0;
            const double l = (0.2126 * m[0]) + (0.7152 * m[1]) + (0.0722 * m[2]);
            const std::array<double, 2> want{128 + (224 * (m[2] - l) / 1.8556),
                                             128 + (224 * (m[0] - l) / 1.5748)};
            for (std::size_t c = 0; c < 2; ++c) {
                const int got = nv12.bytes[uv + (y * nv12.stride) + (2 * x) + c];
                worst[1] =
                    std::max(worst[1], std::abs(got - static_cast<int>(std::lround(want[c]))));
            }
        }
    return worst;
}

} // namespace nv12_reference
