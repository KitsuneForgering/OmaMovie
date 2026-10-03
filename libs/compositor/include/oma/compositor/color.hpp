#pragma once

#include "oma/media/probe.hpp"

#include <array>
#include <cstdint>
#include <optional>

// Color conversion into the working space (proposal for ADR-0006): linear-light BT.709/sRGB
// primaries, D65, premultiplied alpha. HDR transfers are not handled yet (M9).

namespace oma::compositor {

struct Mat3 {
    std::array<double, 9> m{1, 0, 0, 0, 1, 0, 0, 0, 1}; // row-major

    [[nodiscard]] static Mat3 identity() noexcept { return {}; }
    [[nodiscard]] double at(int row, int col) const noexcept;
    [[nodiscard]] Mat3 operator*(const Mat3& o) const noexcept;
    [[nodiscard]] std::array<double, 3> apply(const std::array<double, 3>& v) const noexcept;
    [[nodiscard]] std::optional<Mat3> inverse() const noexcept;
};

// R'G'B' = matrix * (Y, Cb, Cr) + offset, where Y, Cb and Cr are samples normalized to [0, 1]
// of their bit depth (sample / (2^bits - 1)).
struct YuvToRgb {
    Mat3 matrix;
    std::array<double, 3> offset{};
};

// Resolves unspecified values the way players do: BT.709 above 576 lines, BT.601 below;
// YUV without a range is limited range.
[[nodiscard]] YuvToRgb yuv_to_rgb(const media::ColorInfo& color, int bit_depth,
                                  int height) noexcept;

// Converts linear RGB in the source primaries to linear BT.709 primaries (same D65 white).
[[nodiscard]] Mat3 primaries_to_bt709(std::uint8_t primaries, int height) noexcept;

enum class Transfer : std::uint8_t {
    Linear,
    Srgb,   // IEC 61966-2-1
    Bt1886, // gamma 2.4 display EOTF, used for BT.709/601/2020 SDR video
};

// The transfer to decode with; HDR transfers (PQ, HLG) map to Bt1886 and set `hdr`.
[[nodiscard]] Transfer resolve_transfer(std::uint8_t transfer, bool* hdr = nullptr) noexcept;

// Decodes a non-linear value in [0, 1] (clamped) to linear light.
[[nodiscard]] double to_linear(double v, Transfer transfer) noexcept;

} // namespace oma::compositor
