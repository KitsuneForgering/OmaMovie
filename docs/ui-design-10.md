## 10. Visual language

- **Colors**: semantic tokens fed by Omarchy's `colors.toml`.

  | UI token | Omarchy color |
  |---|---|
  | Window background | `background` |
  | Panels (library, drawer) | `dark_background` |
  | Timeline background | `darker_background` |
  | Raised elements (clips, buttons) | `lighter_background` |
  | Selection, playhead, focus | `accent` |
  | Text / secondary text | `foreground` / `dark_foreground` |
  | Clips by kind (video, audio, title) | `blue` / `green` / `magenta` (mixed with the background) |
  | Warnings / errors | `yellow` / `red` |

- **Viewer, thumbnails and scopes are untinted**: they show the real image on a neutral background.
- **Light mode**: follows the theme's `mode` (there are light themes such as `catppuccin-latte` and `flexoki-light`).
- **Density**: flat, few borders, separation by background tone and spacing; slightly rounded corners.
- **Opaque window** (documented Hyprland rule; `omarchy-integration.md`).

### 10.1 Typography (decided)

- **The Omarchy font across the whole UI**, read by the platform adapter when available and updated live by a verified watcher (`omarchy::Theme`; QML text uses `UiText`, never a bare `Text`, so it follows the change). Retain the last valid font during incomplete updates. Fallback outside Omarchy: the system's default monospace
  font (`fontconfig`).
- Consequences of a usually monospaced font:
  - labels take more width: prefer short labels and icons with tooltips in the adjustments bar;
  - timecodes and numbers align naturally (an advantage);
  - hierarchy through **size, weight and color** (`foreground` / `dark_foreground`), not by changing family;
  - test with fonts of different widths (the ones Omarchy offers) so layouts do not break.
- Base size derived from Omarchy's `base-size` (`~/.config/omarchy/shell.toml`, `[font]`) when
  present **(verify that it is the right source for apps)**.

### 10.2 Icons (decided)

- **Our own SVG set**, drawn for OmaMovie (no copying icons from Apple or other editors).
- Set rules:
  - 24×24 px grid, single stroke (e.g. 1.5 px), consistent corners and terminals;
  - **monochrome**: a single color applied at runtime from the theme (normal color, `accent` when active, `dark_foreground` when disabled);
  - filled variant only for active states (e.g. favorite);
  - legible at 16 px.
- Implementation (2026-10-06): each icon is SVG path data in `apps/omamovie/qml/Icon.qml`,
  drawn with Qt Quick Shapes and tinted at runtime. This keeps one source for every size and
  theme without the Qt SVG module; separate `.svg` files would only be needed for places that
  take image files (menu item icons).
- License: the project's own (MIT), with a credits file if any icon derives from another free set.
- The glyphs used in this document's wireframes (◐ ⬚ ♪ ...) are **placeholders only**.

---

[← UI design](ui-design.md)
