## 5. Tests and quality per milestone

| Milestone | Mandatory |
|---|---|
| M0 | Time tests; sanitizers in CI |
| M2 | Media fixtures; probe fuzzing; frame lifetime test |
| M3 | Render graph without a GPU; GPU × CPU comparison |
| M4 | TSan; measured A/V drift |
| M5 | Every operation and undo transaction |
| M6 | Measured UI responsiveness and task-baseline comparison; partial theme update test |
| M7 | Save/load, migrations, failure injection + directory durability; loader fuzzing; independently checked export |
| M8 effects | Registry/command contract; editable stack and presets; CPU/GPU frames, saved-project and decoded-export checks |
| M9 | Fuzzing of every importer; small fixtures per compatibility level |
| M10 | Seeded stateful edits with exact undo/redo; workflow and recovery checks; concurrency and sustained stress; reproducible fuzzing and release evidence |
| M11 | Before/after module inventory; responsive-layout and keyboard walkthrough; affected M10 workflows and performance baselines rerun |

**Long-form gate (proposed, after M7 export, before claiming support):** run the
[declared workload set and protocol](Research/long-form-editing.md#proposed-gate-before-optimizations)
on the reference Intel machine. Include a two-hour source with few cuts, a synthetic
5,000-clip timeline, repeated seeks in long-GOP 4K media, and a costly-effects control.
Record p50/p95/p99 and worst interaction and seek latencies, frame drops, project
open/save phases, peak RAM, cache disk use, decoded-export content/A/V checks and
recovery after failed save/export. The proposed 5,000-clip interaction criterion is
p95 < 100 ms and no routine edit stall > 250 ms; change it only as an explicit
product decision. A passing synthetic gate is evidence for the declared workloads,
not for every long video or hardware configuration. Use the observed dominant cost
to choose between QML model work, decoder/proxy work, preview rendering or project
storage changes. This gate has not run.

Benchmarks (`tools/bench`) always record hardware, driver, FFmpeg version, codec and decode path (`CLAUDE.md` §23).

The [QA strategy](implementation-plan-qa.md) defines the regression rule, distinct
test layers, v0.1 scope and QA-1–QA-6 rollout. It extends these milestone gates;
generated checks complement deterministic regressions. Pull-request CI remains
required for GCC/Clang, sanitizer builds, fuzz smoke, format and tidy.

[← Implementation plan](implementation-plan.md)
