#include "oma/audio/equalizer.hpp"

#include <cmath>
#include <numbers>
#include <vector>

#include "oma_test.hpp"

namespace {

constexpr int kRate = 48000;

// Gain in dB the equalizer applies to a steady sine at `hz` (measured after the filters settle).
double gain_db(const oma::audio::EqBands& bands, double hz) {
    oma::audio::Equalizer eq;
    eq.configure(kRate, bands);
    const std::int64_t frames = kRate; // one second
    std::vector<float> x(static_cast<std::size_t>(frames));
    for (std::int64_t i = 0; i < frames; ++i) {
        x[static_cast<std::size_t>(i)] = static_cast<float>(
            0.25 * std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / kRate));
    }
    eq.process(x, 1, frames, frames);
    double peak = 0.0;
    for (std::int64_t i = frames / 2; i < frames; ++i) {
        peak = std::max(peak, std::abs(static_cast<double>(x[static_cast<std::size_t>(i)])));
    }
    return 20.0 * std::log10(peak / 0.25);
}

oma::audio::EqBands bands(float low, float mid, float high) {
    oma::audio::EqBands b;
    b.low_db = low;
    b.mid_db = mid;
    b.high_db = high;
    return b;
}

std::vector<float> ramp() {
    return std::vector<float>{0.1F, -0.2F, 0.3F};
}

bool near(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

} // namespace

void run_equalizer_tests() {
    describe("audio::Equalizer", {
        it("leaves samples untouched when flat", {
            oma::audio::Equalizer eq;
            eq.configure(kRate, {});
            expect(eq.active()).toBeFalsy();
            std::vector<float> x = ramp();
            eq.process(x, 1, 3, 3);
            expect(x[0] == 0.1F && x[1] == -0.2F && x[2] == 0.3F).toBeTruthy();
        });

        it("boosts and cuts each band where it acts", {
            // Shelves reach their full gain well past the corner; the peak at its center.
            expect(near(gain_db(bands(6.0F, 0.0F, 0.0F), 30.0), 6.0, 0.3)).toBeTruthy();
            expect(near(gain_db(bands(6.0F, 0.0F, 0.0F), 4000.0), 0.0, 0.2)).toBeTruthy();
            expect(near(gain_db(bands(0.0F, -6.0F, 0.0F), 1000.0), -6.0, 0.2)).toBeTruthy();
            expect(near(gain_db(bands(0.0F, -6.0F, 0.0F), 60.0), 0.0, 0.3)).toBeTruthy();
            expect(near(gain_db(bands(0.0F, 0.0F, 6.0F), 18000.0), 6.0, 0.4)).toBeTruthy();
            expect(near(gain_db(bands(0.0F, 0.0F, 6.0F), 200.0), 0.0, 0.2)).toBeTruthy();
        });

        it("filters each channel with its own state", {
            oma::audio::Equalizer eq;
            eq.configure(kRate, bands(12.0F, 0.0F, 0.0F));
            // An impulse on channel 0 only: channel 1 must stay silent.
            std::vector<float> x(64, 0.0F);
            x[0] = 1.0F;
            eq.process(x, 2, 32, 32);
            float right = 0.0F;
            for (std::size_t i = 32; i < 64; ++i) {
                right = std::max(right, std::abs(x[i]));
            }
            expect(right).toEqual(0.0F);
            expect(x[0] != 1.0F).toBeTruthy();
        });
    });
}
