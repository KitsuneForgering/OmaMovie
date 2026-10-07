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

- **What stays intrinsic.** Framing (fit, crop, transform and its keys), opacity, blend mode,
  the Color drawer's adjustments (`ColorAdjust`), grading (`ColorGrade`, ADR-0012) and titles
  (ADR-0015) remain clip properties: each has one fixed place in the pipeline and its own
  drawer. Only operations a user may stack, reorder or bypass become effects.
- **Instances (`libs/timeline`).** `VideoProperties::effects` is an ordered list of at most 16
  `Effect {definition, enabled, params}`: a definition ID string (`oma.look.sepia`,
  `oma.detail`), a bypass flag, and parameter values as `(name, double)` pairs. A definition
  appears at most once per clip, which keeps the stage rules below exact and the UI
  unambiguous, and makes `(ClipId, definition)` the stable address of an instance: no separate
  instance ID is stored, and a split copies the stack without renumbering anything.
- **Definitions (`libs/timeline`, built in).** A static table: ID, display name, stage, and per
  parameter a name, default and closed bounds. `validate()` checks known instances against it.
  There is no package, catalog or registration API: definitions ship with the code until a
  second source of definitions exists. Presets are deferred; when added, a preset expands to
  ordinary instances in one edit.
- **Stages and order.** A definition belongs to one stage. Rendering runs stage by stage, in
  instance order within a stage:
  1. *Detail* (spatial, on the cropped source): `oma.detail` (`amount` in [-1, 1], the former
     `sharpness`).
  2. Color adjustments (intrinsic).
  3. *Look* (per pixel, linear light): `oma.look.<kind>` for black and white, sepia, vintage,
     cool, warm (`amount` in [0, 1], the mix with the original) and `oma.look.vignette`. Looks
     compose into the existing single `Look`: their 3×4 matrices multiply in instance order, and
     the vignette multiplies after them. That composition, not a clamp between looks, is the
     definition both compositors implement.
  4. Grading (intrinsic, ADR-0012).
  The stack's order therefore matters inside a stage; moving an instance across stages is not
  representable, and the UI orders the stack by stage.
- **Mix and bypass.** `enabled = false` skips an instance in evaluation; it stays in the project.
  Mix is a parameter only where a definition declares it (the looks' `amount`); there is no
  generic mix until an effect whose mix differs from its own strength needs one.
- **Animation.** No effect parameter is keyframed yet. When one is, keys follow ADR-0013's rule
  for transform keys (source time), in the same instance.
- **Unknown definitions.** A project may name a definition this build does not know (a newer
  version). The instance loads with its parameters kept verbatim (unknown names included),
  `validate()` accepts it, evaluation skips it, `oma-project validate` and the UI report it,
  and export refuses to start until the user removes or disables it. Saving writes it back
  unchanged. Unknown parameter names on a known definition are refused.
- **Evaluation and render graph.** `timeline::evaluate` carries the stack in the layer's
  properties; the frame builder skips disabled and unknown instances and reads values through
  `effect_param` (a missing parameter takes its default). `compositor::Layer` replaces `filter` with an ordered `looks` list (at most one
  per kind) and keeps `sharpness` for the detail stage; the app maps one onto the other. The
  shaders do not change: composition happens on the CPU in `make_look`.
- **Commands.** Stack edits are pure functions over the list (`with_effect` adds after its
  stage, `with_effect_moved` reorders within it; removal, bypass and values are direct), and
  the result is committed with the existing `edit::set_video`, so each gesture is one undoable
  edit validated by `Timeline::validate`. Instances are addressed by clip and definition, never
  by a list position or display name. QML calls the session (`setClipEffect`,
  `setClipEffectEnabled`, `setClipEffectParam`, `moveClipEffect`) with IDs the session lists
  (`lookEffects`) and holds no definition table.
- **Persistence.** Native format 4: a clip's `video.effects` array of
  `{definition, enabled, params: {name: number}}`. Migration 3 → 4 turns `filter` into
  `oma.look.<kind>` (none: no instance) and a non-zero `sharpness` into `oma.detail`, so old
  projects render the same pixels; a fixture of each look and of blur/sharpen checks it.
  The loader bounds the count, names and values (CLAUDE.md §18); older builds refuse v4.
- **Audio** keeps its own processing chain (EQ, noise reduction); sharing a type name with video
  effects is not a reason to merge them.

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
