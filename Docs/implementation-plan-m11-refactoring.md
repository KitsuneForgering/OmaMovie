### M11 — Targeted ownership and responsive UI

Refactor after M10 establishes behavior and performance baselines; rerun affected
gates afterward. The 2026-10-12 audit found mixed ownership in `Session` (import,
edits, project state, playback and QML mapping), while `main.cpp` mixes startup and
a large GUI smoke. `TimelineView.qml` and `Main.qml` retain context-chain wiring.
Several small Session units already exist. Inspect call paths and change coupling
before extracting anything; preserve timeline, project, GPU and audio contracts.
The former repository-wide 60-line target is retired as a release gate: size is a
search aid, while ownership, behavior and testability determine a useful refactor.

- [ ] Record which selected paths mix responsibilities, change together or are difficult to test; split only where an extraction improves an observed boundary. File counts may locate candidates, but do not drive acceptance. *Recorded (2026-10-14, [inventory](analysis/m11-inventory-2026-10-14.md))*: `session.cpp` headings no longer match their contents (video adjustments under "project files", the transport under "captions"); the transport state is used almost only by six methods, an unowned boundary; features land as Session + header + QML + smoke together. Ranked candidates: a transport owner (needs the threading note first), relabelling/splitting `session.cpp`, and sharing the storyline gesture with `LaneDrag`.
- [ ] Record a baseline: file/function sizes and duplicated behavior in those paths; p50/p95/worst UI interaction latency, playback drops and memory on the existing M6/M10 fixtures; screenshots and keyboard paths at wide, half-width and 640 px with the Omarchy font and larger text. Identify the few changes that will reduce maintenance cost or fix a measured responsiveness issue.
- [ ] Backend: extract a touched `Session` responsibility behind its existing QML-facing API when its owner can be made explicit. Move the smoke driver out of production startup code without changing its launch contract. *Done (2026-10-14)*: the `--smoke` body and its helpers moved verbatim (checked against the previous `main.cpp`) into `gui_smoke.cpp` behind `run_gui_smoke(const SmokeContext&)`, which names the seven startup objects it uses; `main.cpp` went from 2,616 to 682 lines and no longer includes QtTest. `omamovie --smoke` and the CI job are unchanged and pass. Extract project or compositor helpers only for repeated rules or tangled ownership; avoid a parallel service layer.
- [ ] Frontend: extract reusable QML components from repeated clip/lane gestures, menu items and drawer controls. Give components explicit inputs, signals and focus behavior instead of relying on `Main.qml` context-chain IDs. Keep one source of truth for action IDs, selection, snapping and responsive layout state; remove duplicated markup/logic when an extraction replaces it.
- [ ] Make the editor usable at wide, half-width and 640 px, including a larger font: viewer, library, timeline, transport and drawers must reflow or scroll without overlap, clipped essential controls or inaccessible actions. Preserve mouse and keyboard paths, visible focus and direct manipulation. Keep import/probe/cache/export work off the GUI thread, with bounded progress/cancellation, and investigate only measured event-loop stalls.
- [ ] Apply a design pattern only where it removes real repetition or isolates differing behavior: reuse the existing command/undo path for edits and adapters for platform/media boundaries; consider a strategy only when two concrete backends need the same selection contract. Document the local reason and delete the old branches. No new framework, factory or interface for a single implementation.

**Done when**: the touched backend and QML files have smaller, cohesive units with
explicit ownership and less duplicated logic, recorded in a before/after inventory;
the same workflows, project files, undo/redo, exports and keyboard actions pass
their existing checks; the three layout widths and larger text pass a visual and
keyboard walkthrough; measured UI latency, playback and memory do not regress
against the M10 baseline on the same machine/fixtures. Record any accepted
tradeoff with its measurement. A shorter file is not itself a completion criterion.

[← Implementation plan](implementation-plan.md)
