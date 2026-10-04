#include "look.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace oma::compositor {

namespace {

using Row = std::array<double, 4>;
using Matrix = std::array<Row, 3>;

// BT.709 luminance weights: the working space is linear BT.709 (ADR-0006 proposal).
constexpr std::array<double, 3> kLuma{0.2126, 0.7152, 0.0722};

constexpr Matrix kIdentity{{{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}}};

// Mixes toward luminance: 0 grey, 1 unchanged, 2 twice the chroma.
Matrix saturation(double s) {
    Matrix m{};
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            m[r][c] = ((1.0 - s) * kLuma[c]) + (r == c ? s : 0.0);
        }
    }
    return m;
}

// Channel gains scaled so white keeps its luminance: a tint, not a brightness change.
std::array<double, 3> tint(double r, double g, double b) {
    const double y = (kLuma[0] * r) + (kLuma[1] * g) + (kLuma[2] * b);
    return {r / y, g / y, b / y};
}

Matrix diagonal(const std::array<double, 3>& d) {
    Matrix m{};
    for (std::size_t i = 0; i < 3; ++i) {
        m[i][i] = d[i];
    }
    return m;
}

// a * b, with b's offset carried through a.
Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix m{};
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            double v = c == 3 ? a[r][3] : 0.0;
            for (std::size_t k = 0; k < 3; ++k) {
                v += a[r][k] * b[k][c];
            }
            m[r][c] = v;
        }
    }
    return m;
}

// The full look of a filter, before mixing it with the original by its amount.
Matrix filter_matrix(FilterKind kind) {
    switch (kind) {
    case FilterKind::BlackAndWhite:
        return saturation(0.0);
    case FilterKind::Sepia: {
        // Luminance carried by a brown tone.
        const auto t = tint(1.07, 0.74, 0.43);
        Matrix m{};
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                m[r][c] = t[r] * kLuma[c];
            }
        }
        return m;
    }
    case FilterKind::Vintage: {
        Matrix m = multiply(diagonal(tint(1.08, 1.0, 0.82)), saturation(0.65));
        for (Row& row : m) {
            row[3] = 0.012; // lifted blacks, like faded print
        }
        return m;
    }
    case FilterKind::Cool:
        return diagonal(tint(0.9, 1.0, 1.15));
    case FilterKind::Warm:
        return diagonal(tint(1.12, 1.0, 0.85));
    case FilterKind::None:
    case FilterKind::Vignette:
        break;
    }
    return kIdentity;
}

} // namespace

bool Look::identity() const noexcept {
    return gains == std::array<double, 3>{1.0, 1.0, 1.0} && power == 1.0 && matrix == kIdentity &&
           vignette == 0.0;
}

Look make_look(const ColorAdjust& color, const Filter& filter) {
    Look look;
    const double t = color.temperature;
    const auto wb = tint(1.0 + (0.18 * t), 1.0, 1.0 - (0.18 * t));
    const double scale = std::exp2(color.exposure);
    for (std::size_t i = 0; i < 3; ++i) {
        look.gains[i] = wb[i] * scale;
    }
    look.power = std::exp2(color.contrast);
    const Matrix full = filter_matrix(filter.kind);
    Matrix mixed{};
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            mixed[r][c] = kIdentity[r][c] + (filter.amount * (full[r][c] - kIdentity[r][c]));
        }
    }
    look.matrix = multiply(mixed, saturation(1.0 + color.saturation));
    if (filter.kind == FilterKind::Vignette) {
        look.vignette = kVignetteDepth * filter.amount;
    }
    return look;
}

Detail make_detail(double sharpness, std::uint32_t source_height) {
    Detail d;
    if (sharpness == 0.0 || source_height == 0) {
        return d;
    }
    const double scale = static_cast<double>(source_height) / 1080.0;
    double sigma = 0.0;
    if (sharpness < 0.0) {
        d.mode = DetailMode::Blur;
        sigma = -sharpness * 0.02 * static_cast<double>(source_height);
    } else {
        // A fine unsharp mask: about a pixel of radius at 1080p, scaled with the source.
        d.mode = DetailMode::Sharpen;
        sigma = std::max(0.8, 1.2 * scale);
        d.amount = 1.5 * sharpness;
    }
    sigma = std::clamp(sigma, 0.3, static_cast<double>(kMaxBlurRadius) / 3.0);
    d.radius = std::min(kMaxBlurRadius, static_cast<int>(std::ceil(3.0 * sigma)));
    double total = 0.0;
    for (int i = 0; i <= d.radius; ++i) {
        const double w =
            std::exp(-(static_cast<double>(i) * static_cast<double>(i)) / (2.0 * sigma * sigma));
        d.weights[static_cast<std::size_t>(i)] = w;
        total += i == 0 ? w : 2.0 * w;
    }
    for (int i = 0; i <= d.radius; ++i) {
        d.weights[static_cast<std::size_t>(i)] /= total;
    }
    return d;
}

} // namespace oma::compositor
