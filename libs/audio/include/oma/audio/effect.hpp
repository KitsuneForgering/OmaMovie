#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

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

// A clip's effects in processing order, one per slot. Reconfiguring a slot that already holds an
// effect of the same kind keeps its state, so a parameter change during playback does not click.
class EffectChain {
public:
    // The effect in `slot`, created when the slot is empty or holds another kind.
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
    // Drops the effects in slots `count` and beyond.
    void resize(std::size_t count) {
        if (count < effects_.size()) {
            effects_.resize(count);
        }
    }
    [[nodiscard]] std::size_t size() const noexcept { return effects_.size(); }

    [[nodiscard]] bool active() const noexcept;
    void reset() noexcept;
    void process(std::span<float> samples, int channels, std::int64_t stride,
                 std::int64_t frames) noexcept;

private:
    std::vector<std::unique_ptr<Effect>> effects_; // empty slots are skipped
};

} // namespace oma::audio
