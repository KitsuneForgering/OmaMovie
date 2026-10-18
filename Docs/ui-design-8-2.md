**Window and project**
| Shortcut | Action |
|---|---|
| `Ctrl+=` / `Ctrl+-` / `Shift+Z` | Timeline zoom / fit the project |
| `Ctrl+1` | Show/hide the library |
| `Tab` / `Shift+Tab` | Move keyboard focus through the controls (a ring marks it); `Enter` activates. A focused slider or list keeps the arrow keys |
| `Shift+F10` / `Menu` | Context menu of the selected clip; with nothing selected, of the cut under the playhead (transitions) or of the timeline |
| `Ctrl+Shift+A` | Deselect (Final Cut's Deselect All) |
| `Ctrl+I` | Import |
| `Ctrl+E` | Export |
| `Ctrl+Shift+F` | Full-screen viewer |
| `Ctrl+K` | **Command palette** |

- Every shortcut comes from the central action system and is remappable from the command palette (`Ctrl+Enter` on an action); overrides persist in `$XDG_CONFIG_HOME/omamovie/shortcuts.ini`.
- These are **OmaMovie assignments**, originally compiled from memory of the convention. Check action meaning, focus conflicts, accessibility and compositor shortcuts before implementation; do not advertise exact Apple compatibility.
- `Ctrl+K` is the keyword editor in Final Cut; OmaMovie has no keywords, so the palette takes
  the shortcut.

**Audit (2026-10-06).**
- *Conflicts.* Detach audio and Save As both had `Ctrl+Shift+S`; Qt fires neither of two enabled shortcuts with the same keys, so both were dead. Save As keeps it (the desktop convention); Detach audio moved to `Ctrl+Alt+S`. The only shared default left is `Escape` (close the drawer / leave the full-screen viewer), never enabled together; the GUI smoke fails on any other shared default.
- *Compositor.* Omarchy's Hyprland binds outside `SUPER` are `Alt+Tab` (and Shift/Ctrl variants), `Ctrl+Alt+Delete`, `Print`/`Alt+Print` and media/brightness keys (`hyprctl binds`); none is an OmaMovie shortcut.
- *Focus.* Text fields keep printable keys and their editing keys; a focused slider, colour wheel or list keeps the arrows, Home/End (and Space where it toggles); the command palette takes the keyboard while open. Everything else reaches the window's shortcuts.
- *Not assigned yet* (their features do not exist): `/`, `S`, `I`/`O`, `X`, `Ctrl+D`, `M`, the tool keys, the library's `F`/`U`/`Ctrl+F`. Each gets its key when its feature lands, through the same registry.
- Accessibility beyond keyboard reach (screen readers, sticky keys) was not tested.

### 8.2 Command palette (`Ctrl+K`)
- Lists **every** app action, searchable by name ("speed 50%", "export 1080p", "add marker").
- It is disclosure "level 3": any capability is reachable without an on-screen button.
- Matches Omarchy's style (search-driven launcher and menus).
- `Enter` runs the highlighted action (disabled ones are listed dimmed and do not run); `Ctrl+Enter` or its shortcut button records a new shortcut (`Backspace` removes it, `Escape` cancels); a shortcut another action uses is refused with that action's name.

---

[← UI design](ui-design.md)
