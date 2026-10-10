#include "oma/audio/effect.hpp"

#include <algorithm>

namespace oma::audio {

bool EffectChain::active() const noexcept {
    return std::ranges::any_of(effects_, [](const auto& e) { return e && e->active(); });
}

void EffectChain::reset() noexcept {
    for (const auto& e : effects_) {
        if (e) {
            e->reset();
        }
    }
}

void EffectChain::process(std::span<float> samples, int channels, std::int64_t stride,
                          std::int64_t frames) noexcept {
    for (const auto& e : effects_) {
        if (e && e->active()) {
            e->process(samples, channels, stride, frames);
        }
    }
}

std::size_t EffectChain::size() const noexcept { return effects_.size(); }

void EffectChain::resize(std::size_t count) {
    if (count < effects_.size()) {
        effects_.resize(count);
    }
}

} // namespace oma::audio
