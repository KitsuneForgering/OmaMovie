#include "oma/audio/mix.hpp"

#include <array>
#include <cmath>

#include <cmath>

#include <vector>

#include "oma_test.hpp"

using oma::audio::ClipGain;
using oma::audio::mix_planar;

namespace {

ClipGain faded() {
    return ClipGain{.gain = 0.5F, .length = 100, .fade_in = 10, .fade_out = 20};
}

// Two planes of 4 samples: left 1, 2, 3, 4 and right 10, 20, 30, 40.
std::vector<float> stereo_planes() {
    return {1, 2, 3, 4, 10, 20, 30, 40};
}

ClipGain unity(std::int64_t length) {
    return ClipGain{.gain = 1.0F, .length = length, .fade_in = 0, .fade_out = 0};
}

std::vector<float> mono_planes() {
    return {3, 5};
}

// A 1000-sample clip crossfading over 100 samples on each side of both ends.
ClipGain crossfaded() {
    ClipGain g;
    g.length = 1000;
    g.lead = 100;
    g.tail = 100;
    return g;
}

} // namespace

namespace {

// Keyed volume (M8): linear between points, held, eased; before the first and after the last
// the nearest point's gain; fades still multiply on top.
bool automates_gain() {
    using oma::audio::ClipGain;
    using oma::audio::GainPoint;
    const std::array<GainPoint, 3> points{
        GainPoint{.sample = 0, .gain = 1.0F, .interpolation = 1},
        GainPoint{.sample = 100, .gain = 0.0F, .interpolation = 0},
        GainPoint{.sample = 200, .gain = 1.0F, .interpolation = 2}};
    ClipGain g;
    g.length = 400;
    g.automation = points;
    const bool linear = std::abs(g.at(50) - 0.5F) < 1e-6F;
    const bool held = g.at(150) == 0.0F;
    const bool after = g.at(300) == 1.0F;
    g.fade_out = 100; // last 100 samples fade on top of the automation
    return linear && held && after && g.at(399) == 0.0F && std::abs(g.at(350) - 0.49F) < 0.02F;
}

} // namespace

void run_mix_tests() {
    describe("ClipGain", {
        it("follows keyed volume under its fades", { expect(automates_gain()).toBeTruthy(); });
        it("is constant between the fades", {
            expect(faded().at(50)).toBe(0.5F);
            expect(faded().at(10)).toBe(0.5F);
            expect(faded().at(79)).toBe(0.5F);
        });

        it("ramps linearly in from silence", {
            expect(faded().at(0)).toBe(0.0F);
            expect(faded().at(5)).toBe(0.25F);
        });

        it("ramps out to silence at the last sample", {
            expect(faded().at(99)).toBe(0.0F);
            expect(faded().at(89)).toBe(0.25F);
        });

        it("is silent outside the clip", {
            expect(faded().at(-1)).toBe(0.0F);
            expect(faded().at(100)).toBe(0.0F);
        });
    });

    describe("mix_planar", {
        it("interleaves planar stereo and adds to what is there", {
            std::vector<float> out(8, 1.0F);
            const auto in = stereo_planes();
            mix_planar(out, 2, in, 2, 4, 4, unity(4), 0);
            expect(out[0]).toBe(2.0F);
            expect(out[1]).toBe(11.0F);
            expect(out[6]).toBe(5.0F);
            expect(out[7]).toBe(41.0F);
        });

        it("feeds mono to every output channel", {
            std::vector<float> out(4, 0.0F);
            const auto in = mono_planes();
            mix_planar(out, 2, in, 1, 2, 2, unity(2), 0);
            expect(out[0] == 3.0F && out[1] == 3.0F).toBeTruthy();
            expect(out[2] == 5.0F && out[3] == 5.0F).toBeTruthy();
        });

        it("applies the gain at the clip position of each sample", {
            std::vector<float> out(8, 0.0F);
            const auto in = stereo_planes();
            // Samples 98..101 of a 100-sample clip: only 98 is still inside (fading out).
            mix_planar(out, 2, in, 2, 4, 4, faded(), 98);
            expect(out[0]).toBe(0.025F);
            expect(out[2] == 0.0F && out[4] == 0.0F && out[6] == 0.0F).toBeTruthy();
        });

        it("never writes past the output", {
            std::vector<float> out(2, 0.0F);
            const auto in = stereo_planes();
            mix_planar(out, 2, in, 2, 4, 4, unity(4), 0);
            expect(out[0] == 1.0F && out[1] == 10.0F).toBeTruthy();
        });
    });

    describe("audio::ClipGain crossfades", {
        it("plays before and after the clip under equal-power ramps", {
            ClipGain g = crossfaded();
            expect(g.at(-101)).toBe(0.0F);
            expect(g.at(-100)).toBe(0.0F); // sin(0)
            expect(std::abs(g.at(0) - 0.70710678F) < 1e-6F).toBeTruthy();
            expect(g.at(500)).toBe(1.0F);
            expect(g.at(1100)).toBe(0.0F);
            // The outgoing ramp and an incoming ramp at the same instant keep their power.
            ClipGain next;
            next.length = 1000;
            next.lead = 100;
            for (std::int64_t i = -100; i < 100; i += 37) {
                const float out = g.at(1000 + i);
                const float in = next.at(i);
                expect(std::abs((out * out) + (in * in) - 1.0F) < 1e-5F).toBeTruthy();
            }
        });
    });
}
