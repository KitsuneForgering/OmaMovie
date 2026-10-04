#include "oma/audio/effect.hpp"
#include "oma/audio/equalizer.hpp"

#include <vector>

#include "oma_test.hpp"

namespace {

// A second effect kind for the chain: scales samples, counting resets.
class Scale final : public oma::audio::Effect {
public:
    float factor = 1.0F;
    int resets = 0;
    [[nodiscard]] bool active() const noexcept override { return factor != 1.0F; }
    void reset() noexcept override { ++resets; }
    void process(std::span<float> samples, int channels, std::int64_t stride,
                 std::int64_t frames) noexcept override {
        for (int c = 0; c < channels; ++c) {
            for (std::int64_t i = 0; i < frames; ++i) {
                samples[static_cast<std::size_t>((c * stride) + i)] *= factor;
            }
        }
    }
};

std::vector<float> samples() {
    return std::vector<float>{0.5F, -0.25F, 0.125F, 1.0F};
}

oma::audio::EqBands boost() {
    oma::audio::EqBands b;
    b.mid_db = 6.0F;
    return b;
}

} // namespace

void run_effect_tests() {
    describe("audio::EffectChain", {
        it("lets samples through untouched while no effect is active", {
            oma::audio::EffectChain chain;
            chain.at<Scale>(0);
            chain.at<oma::audio::Equalizer>(1).configure(48000, {});
            auto x = samples();
            expect(chain.active()).toBeFalsy();
            chain.process(x, 2, 2, 2);
            expect(x == samples()).toBeTruthy();
        });

        it("runs the active effects in slot order", {
            oma::audio::EffectChain chain;
            chain.at<Scale>(0).factor = 2.0F;
            chain.at<Scale>(1).factor = 0.5F;
            auto x = samples();
            expect(chain.active()).toBeTruthy();
            chain.process(x, 1, 4, 4);
            expect(x == samples()).toBeTruthy();
            chain.at<Scale>(1).factor = 1.0F;
            chain.process(x, 1, 4, 4);
            expect(x[3]).toEqual(2.0F);
        });

        it("keeps an effect of the same kind and replaces one of another kind", {
            oma::audio::EffectChain chain;
            Scale* first = &chain.at<Scale>(0);
            first->factor = 3.0F;
            expect(&chain.at<Scale>(0) == first).toBeTruthy();
            expect(chain.at<Scale>(0).factor).toEqual(3.0F);
            chain.at<oma::audio::Equalizer>(0).configure(48000, boost());
            expect(chain.at<oma::audio::Equalizer>(0).active()).toBeTruthy();
        });

        it("resets every effect and drops slots past a size", {
            oma::audio::EffectChain chain;
            chain.at<Scale>(0);
            chain.at<Scale>(2);
            expect(static_cast<int>(chain.size())).toEqual(3);
            chain.reset();
            expect(chain.at<Scale>(0).resets).toEqual(1);
            chain.resize(1);
            expect(static_cast<int>(chain.size())).toEqual(1);
        });
    });
}
