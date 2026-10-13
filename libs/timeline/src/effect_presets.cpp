#include "oma/timeline/effect_presets.hpp"

#include "oma/timeline/effects.hpp"

#include <algorithm>
#include <array>

namespace oma::timeline {

namespace {
constexpr std::array kOldFilm{PresetEffect{"oma.look.vintage", 0.7},
                              PresetEffect{"oma.look.vignette", 0.5},
                              PresetEffect{"oma.detail", -0.15}};
constexpr std::array kNoir{PresetEffect{"oma.look.black-and-white", 1.0},
                           PresetEffect{"oma.look.vignette", 0.6}, PresetEffect{"oma.detail", 0.2}};
constexpr std::array kWarmGlow{PresetEffect{"oma.look.warm", 0.6},
                               PresetEffect{"oma.detail", -0.2}};
constexpr std::array kCoolClean{PresetEffect{"oma.look.cool", 0.4},
                                PresetEffect{"oma.detail", 0.15}};

constexpr std::array kPresets{
    EffectPreset{.id = "old-film", .name = "Old film", .effects = kOldFilm},
    EffectPreset{.id = "noir", .name = "Noir", .effects = kNoir},
    EffectPreset{.id = "warm-glow", .name = "Warm glow", .effects = kWarmGlow},
    EffectPreset{.id = "cool-clean", .name = "Cool and clean", .effects = kCoolClean},
};
} // namespace

std::span<const EffectPreset> effect_presets() noexcept {
    return kPresets;
}

const EffectPreset* find_effect_preset(std::string_view id) noexcept {
    const auto it = std::ranges::find(kPresets, id, &EffectPreset::id);
    return it == kPresets.end() ? nullptr : &*it;
}

std::vector<Effect> with_preset(std::vector<Effect> effects, const EffectPreset& preset) {
    for (const PresetEffect& p : preset.effects) {
        const EffectDefinition* definition = find_effect_definition(p.definition);
        if (definition == nullptr)
            continue;
        effects = with_effect(std::move(effects), *definition);
        auto it = std::ranges::find(effects, p.definition, &Effect::definition);
        it->enabled = true;
        std::erase_if(it->params, [](const EffectParam& e) { return e.name == "amount"; });
        it->params.push_back({.name = "amount", .value = p.amount});
    }
    return effects;
}

} // namespace oma::timeline
