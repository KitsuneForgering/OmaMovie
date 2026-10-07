#include "oma/timeline/effects.hpp"

#include "mutation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace oma::timeline {

namespace {

constexpr std::array kAmount{
    ParamDefinition{.name = "amount", .min = 0.0, .max = 1.0, .fallback = 1.0}};
// Below 0 a gaussian blur, above 0 an unsharp mask.
constexpr std::array kDetail{
    ParamDefinition{.name = "amount", .min = -1.0, .max = 1.0, .fallback = 0.0}};

constexpr EffectDefinition look(std::string_view id, std::string_view name, FilterKind kind) {
    return {.id = id, .name = name, .stage = EffectStage::Look, .look = kind, .params = kAmount};
}

constexpr std::array kDefinitions{
    EffectDefinition{.id = "oma.detail",
                     .name = "Soften / Sharpen",
                     .stage = EffectStage::Detail,
                     .params = kDetail},
    look("oma.look.black-and-white", "Black & White", FilterKind::BlackAndWhite),
    look("oma.look.sepia", "Sepia", FilterKind::Sepia),
    look("oma.look.vintage", "Vintage", FilterKind::Vintage),
    look("oma.look.cool", "Cool", FilterKind::Cool),
    look("oma.look.warm", "Warm", FilterKind::Warm),
    look("oma.look.vignette", "Vignette", FilterKind::Vignette),
};

std::unexpected<Error> invalid(std::string message) {
    return detail::error(ErrorCode::InvalidData, std::move(message));
}

bool name_ok(std::string_view n) {
    return !n.empty() && n.size() <= kMaxEffectNameBytes;
}

} // namespace

std::span<const EffectDefinition> effect_definitions() noexcept {
    return kDefinitions;
}

const EffectDefinition* find_effect_definition(std::string_view id) noexcept {
    const auto* it = std::ranges::find(kDefinitions, id, &EffectDefinition::id);
    return it == kDefinitions.end() ? nullptr : &*it;
}

double effect_param(const Effect& effect, std::string_view name) noexcept {
    for (const EffectParam& p : effect.params) {
        if (p.name == name) {
            return p.value;
        }
    }
    if (const EffectDefinition* d = find_effect_definition(effect.definition)) {
        for (const ParamDefinition& p : d->params) {
            if (p.name == name) {
                return p.fallback;
            }
        }
    }
    return 0.0;
}

Effect make_effect(const EffectDefinition& definition) {
    Effect e{.definition = std::string(definition.id), .enabled = true, .params = {}};
    for (const ParamDefinition& p : definition.params) {
        e.params.push_back({.name = std::string(p.name), .value = p.fallback});
    }
    return e;
}

std::vector<Effect> with_effect(std::vector<Effect> effects, const EffectDefinition& definition) {
    auto at = effects.begin();
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        if (it->definition == definition.id) {
            return effects;
        }
        const EffectDefinition* d = find_effect_definition(it->definition);
        if (d != nullptr && d->stage <= definition.stage) {
            at = it + 1;
        }
    }
    effects.insert(at, make_effect(definition));
    return effects;
}

std::vector<Effect> with_effect_moved(std::vector<Effect> effects, std::string_view definition,
                                      int step) {
    const auto stage_of = [&](std::size_t i) {
        const EffectDefinition* d = find_effect_definition(effects[i].definition);
        return d == nullptr ? std::optional<EffectStage>{} : d->stage;
    };
    const auto found = std::ranges::find(effects, definition, &Effect::definition);
    if (found == effects.end() || !stage_of(static_cast<std::size_t>(found - effects.begin()))) {
        return effects;
    }
    auto i = static_cast<std::size_t>(found - effects.begin());
    const auto stage = stage_of(i);
    for (; step > 0 && i + 1 < effects.size() && stage_of(i + 1) == stage; --step, ++i) {
        std::swap(effects[i], effects[i + 1]);
    }
    for (; step < 0 && i > 0 && stage_of(i - 1) == stage; ++step, --i) {
        std::swap(effects[i], effects[i - 1]);
    }
    return effects;
}

std::vector<UnknownEffect> unknown_effects(const Timeline& timeline) {
    std::vector<UnknownEffect> out;
    for (const Track& track : timeline.tracks()) {
        for (const Clip& clip : track.clips) {
            for (const Effect& e : clip.video.effects) {
                if (find_effect_definition(e.definition) == nullptr) {
                    out.push_back({.clip = clip.id, .definition = e.definition});
                }
            }
        }
    }
    return out;
}

Result<void> validate_effects(std::span<const Effect> effects) {
    if (effects.size() > kMaxEffects) {
        return invalid("too many effects");
    }
    for (std::size_t i = 0; i < effects.size(); ++i) {
        const Effect& e = effects[i];
        if (!name_ok(e.definition) || e.params.size() > kMaxEffectParams) {
            return invalid("invalid effect");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (effects[j].definition == e.definition) {
                return invalid("effect applied twice: " + e.definition);
            }
        }
        const EffectDefinition* d = find_effect_definition(e.definition);
        for (std::size_t k = 0; k < e.params.size(); ++k) {
            const EffectParam& p = e.params[k];
            if (!name_ok(p.name) || !std::isfinite(p.value)) {
                return invalid("invalid parameter of " + e.definition);
            }
            for (std::size_t m = 0; m < k; ++m) {
                if (e.params[m].name == p.name) {
                    return invalid("parameter set twice in " + e.definition);
                }
            }
            if (d == nullptr) {
                continue; // unknown definition: kept as it is
            }
            const auto known = std::ranges::find(d->params, p.name, &ParamDefinition::name);
            if (known == d->params.end() || p.value < known->min || p.value > known->max) {
                return invalid("parameter " + p.name + " of " + e.definition + " out of range");
            }
        }
    }
    return {};
}

} // namespace oma::timeline
