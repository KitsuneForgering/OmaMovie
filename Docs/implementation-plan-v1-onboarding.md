### First-opening tour and contextual tooltips

**Status: first-opening tour implemented for v0.1 on 2026-10-17; contextual tooltip coverage
and the M6 novice walkthrough remain open.** The tour was brought forward at the maintainer's
request. It teaches the editing model already committed to the core workflow and adds no
editing capabilities. Tests cannot prove that a new editor understands an action's consequences.

**Earlier basis and gap (2026-10-15).** M6 already had a central action registry,
remappable shortcuts, Command Palette, contextual menus and persisted app settings.
`Main.qml` declares `OmaAction` text/keys/handlers; `ActionRegistry` exposes actions and
palette search, but has no focused tooltip text API. Some isolated QML controls use
hover-only `ToolTip`.

**Current implementation.** `WelcomeTour.qml` opens on a fresh profile and presents six
task steps through a first edit: find a command, start/import, mark a source range, place
it, cut/correct, and save/export. It uses the existing actions for names and current
shortcuts. The first step explains the Command Palette and opens it on request. The
second step opens a bundled three-shot demo as an unsaved copy; F1 resumes at the next
step. Finish or
“Don't show again” persists a per-user setting; Escape or “Skip for now” leaves the tour
available on the next launch. F1, the Projects button, the edit toolbar and the Command
Palette reopen it. The popup is modal and changes no `Session` or project state. The GUI
smoke checks first opening, the palette handoff, the demo handoff, Escape, reopening,
completion, persistence, and a 640 px window at 130% text. A headless app test edits the
demo, saves, reopens and independently decodes its exported movie. Human task evidence is
still required.
`Docs/ui-design-2-1.md`, `ui-design-6.md` and `ui-design-10.md` mention tooltips, but do not
specify coverage or keyboard access. Preserve the existing sources of truth rather than
adding a parallel shortcut list.

#### Small, concept-first tour

Plan 4–6 steps, with short text and visible targets or an equivalent compact dialog at
small window sizes. Cover (1) Media import and Omarchy Recordings when available;
(2) Source versus Sequence Viewer, In/Out marks and placing a range; (3) storyline and
connected clips following their anchor; (4) Insert, Overwrite and Connect with the live
shortcuts (`W`, `D`, `Q` by default) and their short consequences; (5) contextual controls
and why capabilities appear with selection; (6) Command Palette, shortcuts and returning
to Help. Avoid teaching roll/slip/slide, color, scopes, effects or export profiles here.

Show on first launch or until completed/dismissed. Provide Skip/Close, Don't show again,
and a Help or Command Palette action to reopen it. Persist the choice in app settings,
outside the project. Do not mutate `Session`, project, timeline or history, require media
or network access, or permanently block the editor. At narrow sizes the content must
reflow/scroll and the exit remains reachable. A small specific component and one setting
are preferred to a generic tutorial engine.

#### Tooltip policy

Cover icon-only controls, unclear contextual controls, structural timeline operations,
precise editing, scopes and important shortcuts. Avoid blanket tooltips. Prefer
`Action name — current shortcut`; omit the separator when there is no shortcut. Add one
short consequence sentence when the action name can be mistaken, for example Overwrite's
non-rippling replacement. Pull name and remapped shortcut from the existing `OmaAction` /
`ActionRegistry` wherever the control represents that action. First inspect the registry
API and document any missing description or lookup contract before adding one; do not
duplicate names/keys in QML. Describe an appropriately disabled action when useful.

Accessibility: tour is fully navigable by keyboard, with predictable focus and Escape
where appropriate; no step requires hover. Tooltips appear on keyboard focus as well as
hover. Check text at supported UI scales, contrast and small windows; do not encode
essential meaning only in color or animation. If animation is used, respect reduced
motion. Do not add elaborate transitions for this feature.

#### Acceptance and evidence

- Bounded app/GUI tests: clean profile shows the tour; finish and dismiss persist across
  restart; manual reopen works; keyboard navigation and close restore a coherent focus;
  a tour session changes no project/Session/history state. Test no-media and narrow-window
  cases. Use representative controls for action name, live remapping, focus tooltip,
  missing shortcut and useful disabled state; avoid screenshot tests for every tooltip.
- Run [M6 walkthrough](m6-walkthrough.md) from a clean profile. Record which operations
  remain unclear after onboarding, absent/wrong/excessive tooltips and whether Help,
  Command Palette and shortcuts can be found again. Repeat T5 keyboard-only with the tour
  closed. Fix ambiguous common UI behavior at its control, rather than expanding the tour.
- Preserve current M6 editing, keyboard, pointer, source-viewer and narrow-layout checks.
  The gate passes only with walkthrough evidence and no blocking confusion in committed
  v1.0 workflows; automated smoke is supporting evidence.

**Out of scope:** a generic lesson framework, online content, advanced VFX instruction,
new editing semantics, and a tooltip on every visual item.

[← Implementation plan](implementation-plan.md)
