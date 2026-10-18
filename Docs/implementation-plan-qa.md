## QA strategy — behavior, state and reproducibility

The goal is to find invalid states, regressions and interactions across subsystems, not
to increase a coverage percentage. Keep explicit deterministic regressions. Generated
tests add breadth; independent reasoning supplies expected values where possible.
Errors preserve project state, edits remain reversible, malformed input cannot silently
corrupt a project, and every failure must be reproducible.

| Layer | Distinct purpose |
|---|---|
| Unit and mathematical invariants | Exact time, edit and rendering rules with independently derived expectations |
| Property and state tests | Explore valid command combinations and check invariants after each step |
| Integration | Cross persistence, media, compositor and job boundaries |
| UI/application workflows | Prove an action changes both the model and visible QML state correctly |
| Fuzzing, sanitizers and static analysis | Find malformed-input, memory, race and static defects |

Avoid repeating the same assertion at every layer without a distinct failure mode.
The required properties are valid state, deterministic editing, exact time behavior,
reversible commands, safe input handling, CPU/GPU agreement, UI/model consistency,
reproducible failures and no silent corruption.

**Regression rule:** Every fixed bug should include a test that failed before the fix,
whenever technically possible. Reproduce → add failing test → fix → retain the test.
Prioritize crashes, corruption, invalid timelines, undo/redo, temporal off-by-one,
frame selection, async races, CPU/GPU divergence, import/export, persistence and UI
desynchronization. Reduce property/fuzzer findings to permanent deterministic cases.
If a regression test is impractical, record why and retain a reproducible case.

**v0.1 required:** preserve deterministic, sanitizer, parser-fuzz, CPU/GPU and M10
workflow/recovery gates for shipped Intel/software paths. Require CI checks before
merging pull requests with GCC and Clang × Debug, Release, ASan/UBSan and TSan;
clang-format, clang-tidy, unit/integration tests and bounded fuzz smoke runs remain
gates. Add the regression rule to reviews. A fast, seeded timeline state/undo test
should join normal CI when QA-2 is ready; it does not replace the current M5 tests.
As audited on 2026-10-12, the workflow runs on PRs but GitHub has no required-check
ruleset; CMake excludes app/Session tests from ASan/TSan. Close these scope gaps
before describing application-level sanitizer or enforced-merge coverage as done.

**Recommended for v0.1:** QA-1 and a bounded QA-2 pilot; a small offscreen UI
workflow pilot if stable on CI. Incomplete QA-3–QA-6 breadth does not block v0.1;
specific failures on shipped paths still block the affected support claim under M10.
**Post-v0.1 infrastructure:** extend the QML driver, structured fuzz targets and
selective golden corpus as those paths ship. **Continuous hardening:** expand command
families, devices, concurrency scenarios and measured performance baselines.

[Timeline properties and reproducibility](implementation-plan-qa-timeline.md) ·
[UI and stateful fuzzing](implementation-plan-qa-ui-fuzz.md) ·
[Rendering, time and differential checks](implementation-plan-qa-render-time.md) ·
[Concurrency, CI and phases](implementation-plan-qa-delivery.md)

[← Implementation plan](implementation-plan.md)
