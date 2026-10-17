#pragma once

// Built-in effect presets (M8): named combinations of the built-in effects (effects.hpp) at set
// amounts. Applying one is a plain stack edit, committed with edit::set_video as one undo entry;
// the result is ordinary editable effects, not a reference to the preset.

#include "oma/timeline/model.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace oma::timeline {

struct PresetEffect {
    std::string_view definition; // an effect definition ID
    double amount = 1.0;         // its "amount" parameter
};

struct EffectPreset {
    std::string_view id; // stable within a build; presets are not persisted
    std::string_view name;
    std::span<const PresetEffect> effects;
};

[[nodiscard]] std::span<const EffectPreset> effect_presets() noexcept;
[[nodiscard]] const EffectPreset* find_effect_preset(std::string_view id) noexcept;

// `effects` with each of the preset's effects enabled at its amount: an instance already in the
// stack is updated in place, a missing one is added where with_effect puts it; others are kept.
[[nodiscard]] std::vector<Effect> with_preset(std::vector<Effect> effects,
                                              const EffectPreset& preset);

} // namespace oma::timeline
