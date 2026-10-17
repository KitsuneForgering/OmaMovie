#include "oma/timeline/effect_presets.hpp"
#include "oma/timeline/effects.hpp"

#include <algorithm>

#include "oma_test.hpp"

namespace tl = oma::timeline;

namespace {
double amount(const std::vector<tl::Effect>& effects, std::string_view definition) {
    const auto it = std::ranges::find(effects, definition, &tl::Effect::definition);
    return it == effects.end() ? -99.0 : tl::effect_param(*it, "amount");
}

// Every preset names known effects within their bounds and applies the same way twice.
bool every_preset_is_valid_and_idempotent() {
    for (const tl::EffectPreset& p : tl::effect_presets()) {
        const auto once = tl::with_preset({}, p);
        if (once.size() != p.effects.size() || !tl::validate_effects(once) ||
            tl::with_preset(once, p) != once)
            return false;
        for (const tl::PresetEffect& e : p.effects)
            if (tl::find_effect_definition(e.definition) == nullptr)
                return false;
    }
    return !tl::effect_presets().empty();
}

// Noir over a stack with sepia and a weak vignette: sepia stays, the vignette is updated in
// place, black and white and sharpening are added.
bool keeps_and_updates_existing_effects() {
    const tl::EffectPreset* noir = tl::find_effect_preset("noir");
    if (noir == nullptr)
        return false;
    std::vector<tl::Effect> stack;
    stack = tl::with_effect(stack, *tl::find_effect_definition("oma.look.sepia"));
    stack = tl::with_effect(stack, *tl::find_effect_definition("oma.look.vignette"));
    stack[1].params.push_back({.name = "amount", .value = 0.2});
    const auto out = tl::with_preset(stack, *noir);
    return out.size() == 4 && amount(out, "oma.look.sepia") == 1.0 &&
           amount(out, "oma.look.vignette") == 0.6 &&
           amount(out, "oma.look.black-and-white") == 1.0 && amount(out, "oma.detail") == 0.2;
}
} // namespace

void run_effect_preset_tests() {
    describe("timeline effect presets", {
        it("names known effects within bounds and applies idempotently",
           { expect(every_preset_is_valid_and_idempotent()).toBeTruthy(); });
        it("keeps other effects and updates ones already in the stack",
           { expect(keeps_and_updates_existing_effects()).toBeTruthy(); });
    });
}
