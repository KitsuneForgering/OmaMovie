#include "oma/compositor/color.hpp"

#include <algorithm>
#include <cmath>

namespace oma::compositor {

namespace {

// ITU-T H.273 code points used below.
constexpr std::uint8_t kMatrixRgb = 0;
constexpr std::uint8_t kMatrixBt709 = 1;
constexpr std::uint8_t kMatrixFcc = 4;
constexpr std::uint8_t kMatrixBt470bg = 5;
constexpr std::uint8_t kMatrixSmpte170m = 6;
constexpr std::uint8_t kMatrixSmpte240m = 7;
constexpr std::uint8_t kMatrixBt2020Ncl = 9;
constexpr std::uint8_t kMatrixBt2020Cl = 10;

constexpr std::uint8_t kPrimariesBt709 = 1;
constexpr std::uint8_t kPrimariesBt470bg = 5;
constexpr std::uint8_t kPrimariesSmpte170m = 6;
constexpr std::uint8_t kPrimariesSmpte240m = 7;
constexpr std::uint8_t kPrimariesBt2020 = 9;
constexpr std::uint8_t kPrimariesP3D65 = 12;

constexpr std::uint8_t kTransferLinear = 8;
constexpr std::uint8_t kTransferSrgb = 13;
constexpr std::uint8_t kTransferPq = 16;
constexpr std::uint8_t kTransferHlg = 18;

constexpr int kSdLines = 576;

struct Chromaticities {
    double rx, ry, gx, gy, bx, by;
};

constexpr double kWhiteX = 0.3127; // D65
constexpr double kWhiteY = 0.3290;

Chromaticities chromaticities(std::uint8_t primaries, int height) {
    switch (primaries) {
    case kPrimariesBt709:
        return {.rx = 0.64, .ry = 0.33, .gx = 0.30, .gy = 0.60, .bx = 0.15, .by = 0.06};
    case kPrimariesBt470bg:
        return {.rx = 0.64, .ry = 0.33, .gx = 0.29, .gy = 0.60, .bx = 0.15, .by = 0.06};
    case kPrimariesSmpte170m:
    case kPrimariesSmpte240m:
        return {.rx = 0.630, .ry = 0.340, .gx = 0.310, .gy = 0.595, .bx = 0.155, .by = 0.070};
    case kPrimariesBt2020:
        return {.rx = 0.708, .ry = 0.292, .gx = 0.170, .gy = 0.797, .bx = 0.131, .by = 0.046};
    case kPrimariesP3D65:
        return {.rx = 0.680, .ry = 0.320, .gx = 0.265, .gy = 0.690, .bx = 0.150, .by = 0.060};
    default:
        // Unspecified or unknown: HD video is BT.709; SD video follows SMPTE 170M.
        return chromaticities(height > kSdLines ? kPrimariesBt709 : kPrimariesSmpte170m, height);
    }
}

// RGB -> XYZ for primaries with a D65 white point.
Mat3 rgb_to_xyz(const Chromaticities& p) {
    const auto xyz = [](double x, double y) {
        return std::array<double, 3>{x / y, 1.0, (1.0 - x - y) / y};
    };
    const auto r = xyz(p.rx, p.ry);
    const auto g = xyz(p.gx, p.gy);
    const auto b = xyz(p.bx, p.by);
    const Mat3 prim{{r[0], g[0], b[0], r[1], g[1], b[1], r[2], g[2], b[2]}};
    const auto white = xyz(kWhiteX, kWhiteY);
    const auto s = prim.inverse().value_or(Mat3::identity()).apply(white);
    return {{r[0] * s[0], g[0] * s[1], b[0] * s[2], r[1] * s[0], g[1] * s[1], b[1] * s[2],
             r[2] * s[0], g[2] * s[1], b[2] * s[2]}};
}

} // namespace

double Mat3::at(int row, int col) const noexcept {
    return m[(static_cast<std::size_t>(row) * 3) + static_cast<std::size_t>(col)];
}

Mat3 Mat3::operator*(const Mat3& o) const noexcept {
    Mat3 r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[(static_cast<std::size_t>(i) * 3) + static_cast<std::size_t>(j)] =
                (at(i, 0) * o.at(0, j)) + (at(i, 1) * o.at(1, j)) + (at(i, 2) * o.at(2, j));
        }
    }
    return r;
}

std::array<double, 3> Mat3::apply(const std::array<double, 3>& v) const noexcept {
    return {(at(0, 0) * v[0]) + (at(0, 1) * v[1]) + (at(0, 2) * v[2]),
            (at(1, 0) * v[0]) + (at(1, 1) * v[1]) + (at(1, 2) * v[2]),
            (at(2, 0) * v[0]) + (at(2, 1) * v[1]) + (at(2, 2) * v[2])};
}

std::optional<Mat3> Mat3::inverse() const noexcept {
    const double a = at(0, 0), b = at(0, 1), c = at(0, 2);
    const double d = at(1, 0), e = at(1, 1), f = at(1, 2);
    const double g = at(2, 0), h = at(2, 1), i = at(2, 2);
    const double co0 = (e * i) - (f * h);
    const double co1 = (f * g) - (d * i);
    const double co2 = (d * h) - (e * g);
    const double det = (a * co0) + (b * co1) + (c * co2);
    if (std::abs(det) < 1e-15) {
        return std::nullopt;
    }
    const double k = 1.0 / det;
    return Mat3{{co0 * k, ((c * h) - (b * i)) * k, ((b * f) - (c * e)) * k, co1 * k,
                 ((a * i) - (c * g)) * k, ((c * d) - (a * f)) * k, co2 * k, ((b * g) - (a * h)) * k,
                 ((a * e) - (b * d)) * k}};
}

YuvToRgb yuv_to_rgb(const media::ColorInfo& color, int bit_depth, int height) noexcept {
    std::uint8_t matrix = color.matrix;
    if (matrix != kMatrixRgb && matrix != kMatrixBt709 && matrix != kMatrixFcc &&
        matrix != kMatrixBt470bg && matrix != kMatrixSmpte170m && matrix != kMatrixSmpte240m &&
        matrix != kMatrixBt2020Ncl && matrix != kMatrixBt2020Cl) {
        matrix = height > kSdLines ? kMatrixBt709 : kMatrixSmpte170m;
    }
    const double max = std::ldexp(1.0, bit_depth) - 1.0; // 2^bits - 1
    const double unit = std::ldexp(1.0, bit_depth - 8);  // 2^(bits - 8)
    const bool full = color.range == media::ColorRange::Full;

    // Normalized samples -> Y' in [0, 1] and Cb/Cr in [-0.5, 0.5].
    const double sy = full ? 1.0 : max / (219.0 * unit);
    const double oy = full ? 0.0 : -16.0 / 219.0;
    const double sc = full ? 1.0 : max / (224.0 * unit);
    const double oc = full ? -std::ldexp(1.0, bit_depth - 1) / max : -128.0 / 224.0;

    if (matrix == kMatrixRgb) {
        // GBR stored as Y=G, Cb=B, Cr=R.
        YuvToRgb out;
        out.matrix = Mat3{{0, 0, sy, sy, 0, 0, 0, sy, 0}};
        out.offset = {oy, oy, oy};
        return out;
    }
    double kr = 0.2126;
    double kb = 0.0722;
    switch (matrix) {
    case kMatrixFcc:
        kr = 0.30;
        kb = 0.11;
        break;
    case kMatrixBt470bg:
    case kMatrixSmpte170m:
        kr = 0.299;
        kb = 0.114;
        break;
    case kMatrixSmpte240m:
        kr = 0.212;
        kb = 0.087;
        break;
    case kMatrixBt2020Ncl:
    case kMatrixBt2020Cl: // constant luminance approximated as non-constant
        kr = 0.2627;
        kb = 0.0593;
        break;
    default:
        break;
    }
    const double kg = 1.0 - kr - kb;
    const Mat3 conv{{1.0, 0.0, 2.0 * (1.0 - kr),                                    //
                     1.0, -2.0 * kb * (1.0 - kb) / kg, -2.0 * kr * (1.0 - kr) / kg, //
                     1.0, 2.0 * (1.0 - kb), 0.0}};
    const Mat3 scale{{sy, 0, 0, 0, sc, 0, 0, 0, sc}};
    YuvToRgb out;
    out.matrix = conv * scale;
    out.offset = conv.apply({oy, oc, oc});
    return out;
}

Mat3 primaries_to_bt709(std::uint8_t primaries, int height) noexcept {
    const Chromaticities src = chromaticities(primaries, height);
    const Chromaticities dst = chromaticities(kPrimariesBt709, height);
    const Mat3 to_xyz = rgb_to_xyz(src);
    const Mat3 from_xyz = rgb_to_xyz(dst).inverse().value_or(Mat3::identity());
    return from_xyz * to_xyz;
}

Transfer resolve_transfer(std::uint8_t transfer, bool* hdr) noexcept {
    if (hdr != nullptr) {
        *hdr = transfer == kTransferPq || transfer == kTransferHlg;
    }
    switch (transfer) {
    case kTransferLinear:
        return Transfer::Linear;
    case kTransferSrgb:
        return Transfer::Srgb;
    default:
        return Transfer::Bt1886;
    }
}

double to_linear(double v, Transfer transfer) noexcept {
    v = std::clamp(v, 0.0, 1.0);
    switch (transfer) {
    case Transfer::Linear:
        return v;
    case Transfer::Srgb:
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    case Transfer::Bt1886:
        return std::pow(v, 2.4);
    }
    return v;
}

} // namespace oma::compositor
