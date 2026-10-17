#include "exporter.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// BT.709 video, the inverse of the BT.1886 EOTF decoding applies (gamma 2.4), so ungraded footage
// keeps its levels (ADR-0006): one table lookup per channel instead of pow(). L^(1/2.4) is
// infinitely steep at black, so the table is indexed by sqrt(L) (output u^(5/6)): over 4096 steps
// that keeps every step under a third of an 8-bit level, where indexing by L lost codes 1-8.
const std::array<std::uint8_t, 4097>& video_table() {
    static const auto table = [] {
        std::array<std::uint8_t, 4097> t{};
        for (std::size_t i = 0; i < t.size(); ++i) {
            const double u = static_cast<double>(i) / 4096.0; // sqrt of linear light
            const double v = std::pow(u, 2.0 / 2.4);
            t[i] = static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
        }
        return t;
    }();
    return table;
}

} // namespace

void encode_bt709(const float* rgba, std::size_t pixels, std::uint8_t* rgb) {
    const auto& table = video_table();
    for (std::size_t p = 0; p < pixels; ++p) {
        // Over the opaque black background the output is opaque: premultiplied colour is the
        // colour (CLAUDE.md §7.4; the background is black in exports).
        for (std::size_t c = 0; c < 3; ++c) {
            const float v = std::clamp(rgba[(p * 4) + c], 0.0F, 1.0F);
            rgb[(p * 3) + c] = table[static_cast<std::size_t>((std::sqrt(v) * 4096.0F) + 0.5F)];
        }
    }
}
