# ADR-0016 — Video effects: an ordered stack of built-in operations

**Status: Accepted (2026-10-06).** The maintainer accepted the stack and native format 4.

## Context

A clip's video effects are fixed members of `VideoProperties`: one `Filter` (a look and its
amount) and one `sharpness` value. `timeline::evaluate` copies them, the app maps them to the
matching `compositor::Layer` fields, and `libs/project` writes them as separate JSON members.
Adding another effect means another member in four places and another branch in QML, and a
clip cannot have two looks or choose their order. The implementation plan asks for an ordered
effect description, defined stage order, bypass, mix, unsupported-effect reporting and a
tested migration before the catalog grows, without a node graph or a plugin API (§2 non-goals).

The two existing operations differ in how they run: a look is per-pixel arithmetic fused into
the layer pass (`src/look.hpp`); blur/sharpen is a spatial pre-pass on the source (reduce,
then two gaussian passes) that the layer pass samples. Both compositors implement both, and the
CPU one is the reference.

## Decision

- [Effect model and stages](0016-stack.md)
- [Evaluation, commands and persistence](0016-integration.md)

## Alternatives considered

- **Keep adding members.** Cheapest per effect, but no order, no stacking, and each effect costs
  a field in the model, the DTO, the render graph and the QML.
- **A per-effect class hierarchy or a generic parameter variant.** Every current parameter is a
  bounded double; strings, colours or curves wait for an effect that needs them.
- **Arbitrary order across stages** (a blur after a look). Needs a separate pass per change of
  stage and intermediate images per layer; no current effect needs it. Reconsider with the
  `ComputeBackend` interface when a multi-pass effect exists.
- **A node graph.** A non-goal (CLAUDE.md §2).
- **Effect packages now.** No second source of definitions exists; the plan's community-pack
  pilot is conditional on M7/M8.

## Consequences

- One look per kind but several kinds per clip, ordered; existing projects keep their pixels.
- `compositor::Layer::looks` is a small vector per layer per frame, joining the per-frame
  allocations already listed in the implementation plan (§8, item 3).
- The Effects drawer lists the clip's stack from the session (add, remove, reorder within a
  stage, bypass, amount) and the clip menu marks a clip with effects as today.
- Export and `oma-project validate` gain the unknown-effect check.
- A new stage or a cross-stage order needs a new ADR, as does any parameter type beyond double.

## Implementation notes (2026-10-07)

Implemented as above: `libs/timeline/include/oma/timeline/effects.hpp` (definitions, stack
edits, `validate_effects`, `unknown_effects`), `compositor::Layer::looks`, native format 4 with
the 3 → 4 migration, `oma-project validate` warnings, and the Effects drawer (look tiles add or
remove a look; the stack shows bypass, amount, order and removal, and lists unknown effects as
unavailable). Export (2026-10-07) refuses to start while unknown effects remain.
