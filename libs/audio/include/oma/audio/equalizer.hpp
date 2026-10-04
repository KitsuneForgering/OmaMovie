#pragma once

#include "oma/audio/effect.hpp"

#include <array>
#include <cstdint>
#include <span>

// A three-band clip equalizer (ui-design §6, Volume "More"): a low shelf, a mid peak and a high
// shelf from the RBJ audio EQ cookbook, run as biquads in transposed direct form II. Plain
// arithmetic on float samples with double state: no allocation, usable from any thread.

namespace oma::audio {

struct EqBands {
    float low_db = 0.0F;  // shelf below kLowHz
    float mid_db = 0.0F;  // peak around kMidHz
    float high_db = 0.0F; // shelf above kHighHz

    friend bool operator==(const EqBands&, const EqBands&) noexcept = default;
};

class Equalizer final : public Effect {
public:
    static constexpr double kLowHz = 120.0;
    static constexpr double kMidHz = 1000.0;
    static constexpr double kHighHz = 8000.0;
    static constexpr int kMaxChannels = 8;

    // Sets the bands for a sample rate, keeping the filter state (a change in the middle of
    // playback does not click). Gains are clamped to ±24 dB.
    void configure(std::int32_t sample_rate, const EqBands& bands) noexcept;
    void reset() noexcept override;
    // Whether any band changes the sound; a flat equalizer passes samples through untouched.
    [[nodiscard]] bool active() const noexcept override { return active_; }
    void process(std::span<float> samples, int channels, std::int64_t stride,
                 std::int64_t frames) noexcept override;

private:
    struct Biquad {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };
    struct State {
        double z1 = 0.0, z2 = 0.0;
    };

    std::array<Biquad, 3> bands_{};
    std::array<std::array<State, 3>, kMaxChannels> state_{};
    bool active_ = false;
};

} // namespace oma::audio
