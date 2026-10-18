## Core inventory — code state at `635ffad`

Labels distinguish implementation from test strength; a risk label is an additional
classification, not a claim of a reproduced defect. `Strongly tested` means diverse
explicit cases on the inspected path, not formal proof or full workflow validation.

| Subsystem | Classification | Evidence and limit |
|---|---|---|
| Timeline tracks/clips, ordering, overlap, IDs | implemented and strongly tested | `libs/timeline/src/model.cpp:630`; `tests/timeline/test_model.cpp`; scale beyond UI 5k needs workload check |
| Connected/anchored clips | implemented but incomplete | `libs/timeline/src/anchors.cpp`; `tests/timeline/test_anchors.cpp`; full cross-command histories ungenerated |
| Edit commands and grouped transactions | implemented and strongly tested | `libs/timeline/src/edit.cpp`, `commands.cpp:46`; explicit edit tests; combined states remain weakly tested |
| Undo/redo and revision | implemented but weakly tested | `editor.cpp:12-138`; `test_edit.cpp:56` compares only compact layout; history memory unbounded |
| Rational/time grid | implemented and strongly tested | `libs/base/src/time.cpp`; `tests/base/test_time.cpp`; UI seconds conversion needs long-form boundary check |
| TimeMap, freeze, reverse, ramps, slicing | implemented and strongly tested | `libs/timeline/src/time_math.cpp`, `model.cpp:51`; `test_time_map.cpp`; composed histories ungenerated |
| Keyframes and automation | implemented but incomplete | `libs/timeline/src/evaluate.cpp`, `tests/timeline/test_scalar_keys.cpp`; color/effect keys and dopesheet absent |
| Transitions | implemented and strongly tested in core | `libs/timeline/src/evaluate.cpp`, `test_edit.cpp`; visible workflow/export content needs cross-check |
| Project format/migrations | implemented and strongly tested | `libs/project/src/document.cpp`, `tests/project/test_document.cpp`; v6, bounds and top-level unknown fields |
| Autosave, versions, relink | implemented but weakly tested | `apps/omamovie/src/session.cpp`, `tests/app/test_project_recovery.cpp`; write interruption/cold relink scale open |
| Recovery of export pair | implemented but incomplete | `tests/app/test_export_crash.cpp`; process-crash checkpoints covered, write/power loss not claimed |
| Fuzzing | implemented but incomplete | `tests/fuzz/CMakeLists.txt`; project, subtitles, LUT only; structured commands/TimeMap absent |
| CI and static checks | implemented but incomplete | `.github/workflows/ci.yml`, `cmake/OmaProjectTargets.cmake:34`; app absent under ASan/TSan |

`Timeline::clip_at` binary-searches sorted clips (`model.cpp:393`); edits validate the
whole timeline. Validation's media/anchor lookups and ripple undo can grow faster
than linearly in clip count. Current evidence does not isolate them as the 5,000-clip
UI bottleneck. Measure edit/validate/undo phases at 1/100/1,000/10,000 clips and
short/long histories before changing containers or using snapshots/COW.

The `Editor`'s typed plan/primitives are a sound baseline: `execute` applies,
reattaches dependents, validates, then records one transaction; failure reverts.
Undo/redo do not revalidate. An independent canonical state oracle and seeded
replay would test that contract more deeply without replacing it. IDs intentionally
remain consumed after undo; revision increments on successful edit/undo/redo.

[← Assessment](omamovie-2026-10-12.md)
