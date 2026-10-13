### M11 — Backend/frontend refactoring and responsive UI

Refactor after M10 establishes a behavior and performance baseline; rerun the
affected M10 gates afterward. Work in small, reviewable slices. The repository-wide
60-line file limit requires semantic partitioning with explicit ownership and stable
behavior. Current candidates include `apps/omamovie/src/session.cpp`
(import, edits, project state and UI mapping), `apps/omamovie/src/main.cpp`
(application startup and a large smoke harness), `apps/omamovie/qml/TimelineView.qml`
(interaction, delegates and menus), and `Main.qml` (window state and wiring).
Inspect call paths and ownership before extracting anything; keep established
timeline, project, GPU and audio contracts intact. On 2026-10-12, 168 tracked text
files still exceed the limit after splitting this plan; the repository gate is open.

- [ ] Partition every tracked text file above 60 lines by cohesive responsibility; keep public interfaces, relative links and test discovery valid. Record the repository-wide count before and after, and inspect changed call paths rather than treating line count as evidence of improved design.
- [ ] Record a baseline: file/function sizes and duplicated behavior in those paths; p50/p95/worst UI interaction latency, playback drops and memory on the existing M6/M10 fixtures; screenshots and keyboard paths at wide, half-width and 640 px with the Omarchy font and larger text. Identify the few changes that will reduce maintenance cost or fix a measured responsiveness issue.
- [ ] Backend: split `Session` by cohesive responsibilities behind its existing QML-facing API, starting with the import/project workflow and edit-to-view mapping only where their dependencies can be made explicit. Move the smoke driver out of production startup code without changing its launch contract. Extract project serialization or compositor helpers only where a focused review finds repeated rules or tangled ownership; avoid a parallel service layer.
- [ ] Frontend: extract reusable QML components from repeated clip/lane gestures, menu items and drawer controls. Give components explicit inputs, signals and focus behavior instead of relying on `Main.qml` context-chain IDs. Keep one source of truth for action IDs, selection, snapping and responsive layout state; remove duplicated markup/logic when an extraction replaces it.
- [ ] Make the editor usable at wide, half-width and 640 px, including a larger font: viewer, library, timeline, transport and drawers must reflow or scroll without overlap, clipped essential controls or inaccessible actions. Preserve mouse and keyboard paths, visible focus and direct manipulation. Keep import/probe/cache/export work off the GUI thread, with bounded progress/cancellation, and investigate only measured event-loop stalls.
- [ ] Apply a design pattern only where it removes real repetition or isolates differing behavior: reuse the existing command/undo path for edits and adapters for platform/media boundaries; consider a strategy only when two concrete backends need the same selection contract. Document the local reason and delete the old branches. No new framework, factory or interface for a single implementation.

**Done when**: the touched backend and QML files have smaller, cohesive units with
explicit ownership and less duplicated logic, recorded in a before/after inventory;
the same workflows, project files, undo/redo, exports and keyboard actions pass
their existing checks; the three layout widths and larger text pass a visual and
keyboard walkthrough; measured UI latency, playback and memory do not regress
against the M10 baseline on the same machine/fixtures. Record any accepted
tradeoff with its measurement. Every tracked text file is at most 60 lines;
that gate and behavior/performance evidence are reported separately.

[← Implementation plan](implementation-plan.md)
