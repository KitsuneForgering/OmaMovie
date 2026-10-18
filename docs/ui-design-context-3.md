# Historical shell state (part 2)

- Keyboard path without a mouse (2026-10-04): every button takes Tab focus with a visible
  ring, Enter activates it, Space keeps playing; ↑/↓ select clips, Shift+F10 opens the
  clip menu. Library and clip names are 12/11 px (were 10).
- Nothing is saved before M7, so the Projects screen shows "Current session" with a
  visible warning, and New project or closing the window asks before discarding a session
  that has media.
- Drag and drop (§7.3): dragging a storyline clip moves it to the nearest cut (one
  "Move" in the history; transitions of the separated clip pairs are dropped), and
  library items drag onto the timeline: pictures insert at the nearest cut, sound
  lands on the lane under the pointer (or the first lane with room, or a new one). An
  accent bar marks the cut while dragging.
- Context menus (§7.3): right-clicking a storyline clip offers the adjustments that
  apply to it (each opens its drawer), Split here, Detach audio, Delete and Replace
  with a gap; a lane clip offers Volume, Info, Split here and Delete; the ⋈ marker
  opens the transition menu; a library item offers Append, Insert and Overwrite, which the
  library header also shows for the selected item (Add, and ▾ for the others). They use
  the same actions as the adjustments bar and the shortcuts.
- Commands are `OmaAction`s in one registry in `Main.qml` with the §8.1 keys
  (Space, K, L, arrows, Shift+arrows, Home/End, E, W, D, Ctrl+B, Delete,
  Shift+Delete, Ctrl+Z, Ctrl+Shift+Z, Ctrl+I, Ctrl+1, Ctrl+Shift+F, Escape,
  Ctrl+=/−, Shift+Z). Ctrl+Shift+I toggles Info; J (reverse) waits for reverse
  playback and Q (connect above) for a second video track. `make run-gui
  RUN_GUI_SMOKE=1` drives import, every edit with undo/redo, playback and the plain
  keys (through Qt's shortcut map) when the window gets focus.
- Icons follow §10.2 (24 px grid, 1.5 px stroke, tinted at runtime) but are drawn
  as SVG path data with Qt Quick Shapes (`apps/omamovie/qml/Icon.qml`): shipping
  `.svg` files through the Qt Resource System needs `qt6-svg`, a dependency not
  yet approved.

---

[← UI design](ui-design.md)
