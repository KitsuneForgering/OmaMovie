#pragma once

// The built-in video effect definitions (ADR-0016). Effects are plain values in the model;
// this table gives their stage, parameters and bounds. There is no registration API: a new
// effect is a new row here plus its renderer.

#include "oma/base/error.hpp"
#include "oma/timeline/model.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace oma::timeline {

// Where an effect runs. Stages render in this order, instances in stack order within one.
enum class EffectStage : std::uint8_t {
    Detail, // spatial, on the cropped source, before the color adjustments
    Look,   // per pixel, after the color adjustments and before grading
};

struct ParamDefinition {
    std::string_view name;
    double min = 0.0;
    double max = 1.0;
    double fallback = 0.0; // the value when an instance does not set it
};

struct EffectDefinition {
    std::string_view id; // stable, persisted: never rename
    std::string_view name;
    EffectStage stage = EffectStage::Look;
    FilterKind look = FilterKind::None; // the look a Look-stage effect applies
    std::span<const ParamDefinition> params;
};

[[nodiscard]] std::span<const EffectDefinition> effect_definitions() noexcept;
// nullptr when this build does not know the definition.
[[nodiscard]] const EffectDefinition* find_effect_definition(std::string_view id) noexcept;
// The instance's value for `name`, else the definition's default (0 when neither has it).
[[nodiscard]] double effect_param(const Effect& effect, std::string_view name) noexcept;
// An enabled instance with every parameter at its default.
[[nodiscard]] Effect make_effect(const EffectDefinition& definition);

// Stack edits (ADR-0016); the caller commits the result with edit::set_video, one undo entry.
// `definition` with its defaults after the last effect of its stage or an earlier one; the list
// unchanged when it already has it.
[[nodiscard]] std::vector<Effect> with_effect(std::vector<Effect> effects,
                                              const EffectDefinition& definition);
// The effect named `definition` moved by `step` places, staying among effects of its stage;
// unknown definitions do not move.
[[nodiscard]] std::vector<Effect> with_effect_moved(std::vector<Effect> effects,
                                                    std::string_view definition, int step);

// Effects this build cannot render, for the report that `oma-project validate`, the UI and
// export show before a project renders differently from how it was made (ADR-0016).
struct UnknownEffect {
    ClipId clip;
    std::string definition;
};
[[nodiscard]] std::vector<UnknownEffect> unknown_effects(const Timeline& timeline);

// Bounds, unique definitions and names; known definitions also check parameter names and
// ranges. Unknown definitions pass so a newer project's effects survive a round trip.
[[nodiscard]] Result<void> validate_effects(std::span<const Effect> effects);

} // namespace oma::timeline
