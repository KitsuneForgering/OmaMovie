#pragma once

#include "oma/audio/effect.hpp"

#include <concepts>
#include <memory>
#include <vector>

namespace oma::audio {
// A clip's effects in processing order, one per slot. Reconfiguring a slot that already holds an
// effect of the same kind keeps its state, so a parameter change during playback does not click.
class EffectChain {
public:
    template <std::derived_from<Effect> E>
    E& at(std::size_t slot) {
        if (slot >= effects_.size()) {
            effects_.resize(slot + 1);
        }
        if (auto* existing = dynamic_cast<E*>(effects_[slot].get())) {
            return *existing;
        }
        auto made = std::make_unique<E>();
        E& ref = *made;
        effects_[slot] = std::move(made);
        return ref;
    }
    void resize(std::size_t count);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    void reset() noexcept;
    void process(std::span<float> samples, int channels, std::int64_t stride,
                 std::int64_t frames) noexcept;

private:
    std::vector<std::unique_ptr<Effect>> effects_;
};
} // namespace oma::audio
