## QA — concurrency, CI and incremental delivery

Exercise temporal interactions with controlled barriers, cancellation and deadlines:
close a project during import; rapid seek during decode; switch clips while an old
frame renders; edit during playback; repeated open/close; cache invalidation;
background imports; concurrent project snapshots; shutdown during a job. Assert
latest-request wins where specified, no stale frame or wrong project is published,
acknowledged edits survive, queues remain bounded and teardown finishes. Run under
TSan where supported, plus ASan/UBSan for lifetime paths. Preserve the Editor's
single-thread ownership: workers receive snapshots, never borrowed live state.
Use deterministic synchronization for race regressions, then longer stress loops.

CI on every pull request remains mandatory: GCC and Clang; Debug, Release,
ASan/UBSan and TSan; format, tidy, unit/integration and bounded fuzz smoke runs.
Fast fixed-seed properties join normal `make test` once stable. Keep categories
`fast`, `integration`, `gpu`, `fuzz`, `stress`, `performance` only when scheduling
needs them; `fast` must cover ordinary edit and persistence failures. Hardware,
long fuzz and stress runs have recorded seeds, commands, fixtures and versions.
Never remove a deterministic regression to make a randomized job pass.

Performance correctness is separate from timing. Benchmark timeline evaluation,
compositing, decode, frame cache, seeking, thumbnails and large projects using fixed
workloads, warm/cold conditions and recorded hardware/software versions. Compare
relative to a same-machine baseline; use broad bounds only for absurd regressions.
Do not turn variable absolute latency into a flaky correctness assertion. Preserve
the existing M6/M10 long-form measurements and investigate changes before release.

| Phase | Smallest useful increment | Scope |
|---|---|---|
| QA-1 | Regression policy, semantic timeline printer, invariant helpers, seed/replay record | Recommended v0.1 |
| QA-2 | Bounded command generator, exact undo/redo states, shrinking | Pilot recommended v0.1; breadth continuous |
| QA-3 | Offscreen QML driver, IDs for used controls, Razor/effect/source workflows | Small pilot recommended; broad harness post-v0.1 |
| QA-4 | Structured timeline/project fuzz, ASan/UBSan, regression conversion | Existing parser gates required; expansion continuous |
| QA-5 | CPU/GPU cases, selective goldens, color and external oracles | Existing shipped-path gates required; breadth continuous |
| QA-6 | Race scenarios, large projects and performance baselines | Existing M10 gates required; expanded infrastructure continuous |

Each phase reuses the current `Editor`, native format, `Session`, `ActionRegistry`,
offscreen tests, parser fuzzers and compositor reference. Add a helper only after a
specific untested boundary is identified; keep it test-local unless a stable public
capability also serves the app. The phases are sequencing guidance, not a blanket
v0.1 release prerequisite. M10 still decides support claims for shipped paths.

[← QA strategy](implementation-plan-qa.md)
