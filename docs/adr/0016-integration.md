## Evaluation, commands and persistence

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

[Return to 0016-video-effects.md](0016-video-effects.md)
