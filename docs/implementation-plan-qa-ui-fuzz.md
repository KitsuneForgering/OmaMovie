## QA — application workflows and stateful fuzzing

Extend the existing offscreen `tests/app` Session tests and the Qt Test smoke embedded
in `main.cpp` with a small driver for the real QML tree. Reuse `ActionRegistry`, public
`Session` methods and existing edit commands; add only stable lookup IDs, fixture
loading and bounded async wait helpers. A pure Session test cannot detect stale QML
bindings or a wrong action route; the driver must inspect both model and visible UI.
Use offscreen CI where rendering is reliable and a real-window hardware check where
the platform compositor or GPU matters. Do not build a second edit API for tests.

Driver abilities, added as workflows need them: open project/load fixture; trigger
an action; select a clip; move the playhead; interact with QML controls and menus;
edit a property; undo/redo through the UI; await jobs with a deadline; query public
Session state and visible QML state. Fail on timeout with action, selection, job and
UI diagnostics. Exercise the same route a user invokes, including keyboard and
menu paths where they have distinct wiring.

Initial workflows: Razor at an exact playhead → two visible clips → UI Undo restores
both timeline and delegates; apply an effect → model instance and Effect Controls
agree → Undo clears both; source viewer In/Out → append/insert/overwrite exact
source range → Undo/Redo agrees. Expand to trim, roll, slip, slide, ripple delete,
connected clips, transitions, keyframes and speed ramps as each route ships. Check
model state, QML selection/controls and visible result where relevant, so a passing
backend command cannot mask a stale view or a wrong action target.

Give actions semantic IDs in the existing `ActionRegistry`; give QML elements stable
`objectName`/automation IDs where an action ID is insufficient. Examples:
`timeline.tool.razor`, `timeline.clip.<id>`, `effects.gaussian_blur`,
`effect_controls.opacity`, `transport.play`, `menu.edit.undo`,
`source.mark_in`, `source.mark_out`. Derive dynamic clip IDs from model IDs.
Never locate by screen position, translated text or unstable delegate index.
Use the same IDs for accessibility and future agent automation, without a duplicate
registry. `ActionRegistry::trigger(id)` covers discrete actions today; parameterized
edits still call `Session`. Add a typed parameter schema only for a proven CLI/agent
workflow, keeping core commands typed. Introduce IDs only for controls needed now.

Expand current parser fuzzing to stateful project load/save and serialized project
mutations, command sequences, time maps, interchange/import formats, metadata and
caption import/export as those formats exist. Prefer structured timeline operations
and valid-prefix mutation over arbitrary bytes when they reach deeper state:
generated timeline → command log → `Editor` → invariants → undo all → redo all.
For serialized inputs, keep invalid/truncated/boundary corpus cases and verify clean
rejection, state preservation, bounded allocation/work and round-trip stability.
Run targets under ASan/UBSan with explicit budgets and seeds. Emit the same exact
replay artifact as property tests; reduce every finding to a deterministic regression
before closing it. A target is added for an implemented input/state path, not for a
speculative format.

The UI driver is a small seam because existing Session tests do not inspect the QML
tree, while the GUI smoke does not offer reusable action/selection assertions. The
stateful fuzzer reuses timeline fixtures and replay rather than introducing another
timeline model. Keep both harnesses in tests and avoid production hooks unless the
same public affordance benefits accessibility or ordinary automation.

[← QA strategy](implementation-plan-qa.md)
