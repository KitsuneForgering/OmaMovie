#include "oma/audio/equalizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace oma::audio {

namespace {

enum class Shape : std::uint8_t {
    LowShelf,
    Peak,
    HighShelf,
};

// Coefficients from Robert Bristow-Johnson's "Cookbook formulae for audio EQ biquad filter
// coefficients", normalized by a0. Shelves use slope S = 1; the peak uses Q = 0.9 (about 1.5
// octaves, gentle enough for voice and music).
struct Coefficients {
    double b0, b1, b2, a1, a2;
};

Coefficients design(Shape shape, double rate, double hz, double db) {
    const double a = std::pow(10.0, db / 40.0);
    const double w0 = 2.0 * std::numbers::pi * std::min(hz, rate * 0.45) / rate;
    const double cosw = std::cos(w0);
    const double sinw = std::sin(w0);
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 1.0;
    double a1 = 0.0;
    double a2 = 0.0;
    switch (shape) {
    case Shape::Peak: {
        const double alpha = sinw / (2.0 * 0.9);
        b0 = 1.0 + (alpha * a);
        b1 = -2.0 * cosw;
        b2 = 1.0 - (alpha * a);
        a0 = 1.0 + (alpha / a);
        a1 = -2.0 * cosw;
        a2 = 1.0 - (alpha / a);
        break;
    }
    case Shape::LowShelf:
    case Shape::HighShelf: {
        const double alpha = sinw / 2.0 * std::numbers::sqrt2; // S = 1
        const double k = 2.0 * std::sqrt(a) * alpha;
        const double s = shape == Shape::LowShelf ? 1.0 : -1.0;
        b0 = a * ((a + 1.0) - (s * (a - 1.0) * cosw) + k);
        b1 = s * 2.0 * a * ((a - 1.0) - (s * (a + 1.0) * cosw));
        b2 = a * ((a + 1.0) - (s * (a - 1.0) * cosw) - k);
        a0 = (a + 1.0) + (s * (a - 1.0) * cosw) + k;
        a1 = -s * 2.0 * ((a - 1.0) + (s * (a + 1.0) * cosw));
        a2 = (a + 1.0) + (s * (a - 1.0) * cosw) - k;
        break;
    }
    }
    return {.b0 = b0 / a0, .b1 = b1 / a0, .b2 = b2 / a0, .a1 = a1 / a0, .a2 = a2 / a0};
}

} // namespace

void Equalizer::configure(std::int32_t sample_rate, const EqBands& bands) noexcept {
    const auto clamp = [](float db) {
        return std::clamp(static_cast<double>(db), -24.0, 24.0);
    };
    const double rate = static_cast<double>(std::max(sample_rate, 1));
    const std::array<Coefficients, 3> c{
        design(Shape::LowShelf, rate, kLowHz, clamp(bands.low_db)),
        design(Shape::Peak, rate, kMidHz, clamp(bands.mid_db)),
        design(Shape::HighShelf, rate, kHighHz, clamp(bands.high_db))};
    for (std::size_t i = 0; i < bands_.size(); ++i) {
        bands_[i] =
            Biquad{.b0 = c[i].b0, .b1 = c[i].b1, .b2 = c[i].b2, .a1 = c[i].a1, .a2 = c[i].a2};
    }
    active_ = bands.low_db != 0.0F || bands.mid_db != 0.0F || bands.high_db != 0.0F;
}

void Equalizer::reset() noexcept {
    state_ = {};
}

void Equalizer::process(std::span<float> samples, int channels, std::int64_t stride,
                        std::int64_t frames) noexcept {
    if (!active_ || channels <= 0 || frames <= 0) {
        return;
    }
    const int n = std::min(channels, kMaxChannels);
    for (int ch = 0; ch < n; ++ch) {
        const auto base = static_cast<std::size_t>(ch) * static_cast<std::size_t>(stride);
        if (base + static_cast<std::size_t>(frames) > samples.size()) {
            return;
        }
        auto& st = state_[static_cast<std::size_t>(ch)];
        for (std::int64_t i = 0; i < frames; ++i) {
            auto& sample = samples[base + static_cast<std::size_t>(i)];
            auto x = static_cast<double>(sample);
            for (std::size_t b = 0; b < bands_.size(); ++b) {
                const Biquad& q = bands_[b];
                State& z = st[b];
                const double y = (q.b0 * x) + z.z1;
                z.z1 = (q.b1 * x) - (q.a1 * y) + z.z2;
                z.z2 = (q.b2 * x) - (q.a2 * y);
                x = y;
            }
            sample = static_cast<float>(x);
        }
    }
}

} // namespace oma::audio
