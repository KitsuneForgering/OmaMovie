## Effect model and stages

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

[Return to 0016-video-effects.md](0016-video-effects.md)
