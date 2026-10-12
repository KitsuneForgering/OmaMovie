#pragma once

#include <cstdint>
#include <span>

// Clip audio effects (CLAUDE.md §12): processors that change a clip's samples in place, in
// planar float32, before the clip is mixed. Each effect keeps its own parameters (set by its own
// configure function) and filter state; the chain runs them in order.
//
// Threading: process() runs on the playback audio pipeline thread, never the real-time callback,
// and must not allocate or block, so a later move to the callback stays possible. Building or
// changing a chain (EffectChain::at, resize) allocates and belongs outside process().

namespace oma::audio {

class Effect {
public:
    virtual ~Effect() = default;

    // Whether the effect changes the sound with its current parameters; an inactive effect is
    // skipped, and a chain of inactive effects lets the samples through untouched.
    [[nodiscard]] virtual bool active() const noexcept = 0;
    // Forgets the filter state: after a seek, the next sample is not a continuation.
    virtual void reset() noexcept = 0;
    // Processes `frames` samples of planar input in place: plane c at samples[c * stride ...].
    virtual void process(std::span<float> samples, int channels, std::int64_t stride,
                         std::int64_t frames) noexcept = 0;

protected:
    Effect() = default;
    Effect(const Effect&) = default;
    Effect& operator=(const Effect&) = default;
    Effect(Effect&&) = default;
    Effect& operator=(Effect&&) = default;
};

} // namespace oma::audio

#include "oma/audio/effect_chain.hpp"
