# UI design — OmaMovie

> Version of 2026-10-02, with the decisions in §12. Based on `Docs/Research/imovie.md`
> (principles), `movie-maker.md`, `capcut.md`, `davinci-resolve.md` §4 and
> `omarchy-integration.md`. Implementation rules remain in `CLAUDE.md` §11.
>
> Inspired by iMovie **in its principles**, without copying Apple's visual identity, icons,
> feature names or trade dress.

---

## 1. Principles

| # | Principle | Origin |
|---|---|---|
| 1 | **Three fixed areas**: library, viewer, timeline. No floating panels or workspaces | iMovie |
| 2 | **Controls appear in context**: the selection decides what can be adjusted | iMovie (adjustments bar), Clipchamp |
| 3 | **Manipulate in the preview**: position, scale and crop by dragging on the image; numbers are for precision | iMovie |
| 4 | **Magnetic timeline by default**: no accidental gaps; the underlying model stays generic multitrack | iMovie / FCP |
| 5 | **Intent presets**: "Picture-in-picture", not "layer + transform + mask"; once applied, everything stays editable | iMovie, CapCut |
| 6 | **Hide, never remove**: advanced features are one click or shortcut away, but do not take up the screen | The iMovie '08 lesson |
| 7 | **Keyboard first**: everything reachable by shortcut and through the command palette | Omarchy culture, Premiere |
| 8 | **The interface follows the Omarchy theme; the image never does** | `omarchy-integration.md` §3–4 |
| 9 | **Works at half width**: in Hyprland the window lives in a tiling layout | Omarchy |

---

## 2. Layout

### 2.1 Wide window (default, ≥ ~1500 px)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ◂ Projects   My vlog ▾                        ⟲ ⟳     ⤓ Import     ⇪ Export   │  top bar
├───────────────────────────────┬──────────────────────────────────────────────┤
│ LIBRARY                       │  ◐ Color  ⬚ Crop  ♪ Volume  ⏱ Speed  ✦ Effects  ⧉ Overlay  ⓘ │  adjustments bar
│ ┌───────────┐                 ├──────────────────────────────────────────────┤
│ │ Project   │ Media  Titles   │  [drawer with the active adjustment]         │  (only when one is open)
│ │ Recordings●│ Transitions Audio├────────────────────────────────────────────┤
│ │ Videos    │                 │                                              │
│ │ Favorites │ ▢▢▢▢  ▢▢▢▢      │                 VIEWER                       │
│ │ + Folder  │ ▢▢▢▢  ▢▢▢▢      │          (neutral background, no theme)      │
│ └───────────┘ ▢▢▢▢  ▢▢▢▢      │                                              │
│               (skimming)      │   00:01:12:08 / 00:04:30:00     ◂◂  ▶  ▸▸   ⤢ │  transport
├───────────────────────────────┴──────────────────────────────────────────────┤
│ ▁▂▃▅▇▅▃▂▁▁▂▃▅▃▂▁▁▂▃▅▇▅▃▂▁  ← minimap (whole project, visible region)          │
│ ┊   T  [ Title "Welcome" ]                                                   │  layers above
│ ┊   V      [ PiP webcam ]                                                     │
│ ┊ ▶ [ clip 1 ][⋈][ clip 2      ][⋈][ clip 3 ][ clip 4  ]   ← primary storyline│  (magnetic)
│ ┊ ♪ ~~~~~~~~~~ music ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                           │  audio below
│ ┊ 🎙   ~~ voiceover ~~                                                         │
│ ┊                                     🔍 ──●──   ⌁ snap                       │
└──────────────────────────────────────────────────────────────────────────────┘
```

- **Default split**: top ~55%, timeline ~45%. **A single** horizontal divider resizes everything (iMovie).
- **Library** on the left, ~30% of the width; **viewer** on the right, as large as possible.
- The top bar only has: back to Projects, project name, undo/redo, import, export.

### 2.2 Half width (tiling, ~900–1500 px)

```
┌────────────────────────────────────────────┐
│ ◂  My vlog ▾                 ⤓   ⇪          │
├────────────────────────────────────────────┤
│ ◐ ⬚ ♪ ⏱ ✦ ⧉ ⓘ                       ▤ Library│  ← the library becomes an overlay panel (Ctrl+1)
├────────────────────────────────────────────┤
│                 VIEWER                     │
├────────────────────────────────────────────┤
│ minimap                                    │
│ timeline                                   │
└────────────────────────────────────────────┘
```

- The library becomes a **sliding panel** over the viewer (shortcut `Ctrl+1`), closing once a clip is added.
- Adjustments bar labels become icons only (with a tooltip and shortcut).

### 2.3 Narrow window (< ~900 px)

Viewer on top, timeline below, library and controls as overlay panels. Usable, not optimized.

---

## 3. Two screens: Projects and Edit

As in iMovie, there is a **Projects** screen and the **Edit** screen.

### 3.1 Which screen opens (decided)

It depends on **how** the app was opened:

| Origin | Opens in |
|---|---|
| Launcher, Omarchy menu, `omamovie` without arguments | **Projects** |
| A project file (double click, "Open with", `omamovie project.<ext>`) | **Edit** for that project |
| One or more videos ("Open with OmaMovie", `omamovie video.mp4`) | **Edit** for a new project with the videos on the timeline (format from the first clip) |
| "Edit" on a recording (Projects screen or, later, an Omarchy notification) | **Edit** for a new project with the recording |
| App already open and receiving a file | Same rule, in the existing window (single instance) |

On the Edit screen, "◂ Projects" always returns to the list.

**Projects**
```
┌──────────────────────────────────────────────────────────────┐
│  OmaMovie                                     + New project    │
│                                                              │
│  Recent                                                      │
│  ▢ My vlog         ▢ Hyprland tutorial    ▢ Short #12 (9:16)  │
│                                                              │
│  Recent Omarchy recordings                          see all ›│
│  ▢ today 10:42     ▢ yesterday 21:03      ▢ yesterday 18:10  │
│     [ Edit ]                                                 │
└──────────────────────────────────────────────────────────────┘
```
- **"Edit" on a recording** creates a project with it already on the timeline: the shortcut for the record (ALT+PRINT) → edit flow.
- **New project** asks one thing only: the format (16:9 horizontal, 9:16 vertical, 1:1 square), defaulting to 16:9. Resolution and frame rate come from the first clip (editable later).

---

## 4. Library

- **Sources** (collapsible list on the left): Project (media already used), **Recordings** (Omarchy's video folder, with a new-items indicator), Videos, Favorites, folders added by the user.
- **Content tabs**: Media, Titles, Transitions, Audio (the user's local sound effects and music). No stock/cloud content.
- **Thumbnail grid** with adjustable size (`Ctrl+scroll`).
- **Skimming**: hovering scrubs through the clip in the viewer without playing. With the keyboard: arrows move the selection, `Space` plays.
- **Selecting a range**: dragging over a thumbnail marks In/Out (or `I`/`O`); the marked range is what goes to the timeline.
- Visual markers: range already used in the project (thin bar), favorite, rejected.
- `F` favorites, `Delete` rejects (does not delete the file), `U` clears the rating, `Ctrl+F` searches.

---

## 5. Viewer

- **Fixed neutral background** (dark gray), independent of the Omarchy theme.
- **Direct manipulation** when an adjustment is active: position/scale/rotation handles (Transform), crop rectangle (Crop), eyedropper (Color).
- **Optional guides**: safe area, thirds, format outline (useful when reframing 16:9 → 9:16).
- **Transport**: current/total timecode, back, play, forward, full screen. Large buttons only in full screen; minimal in the window.
- **Real-time indicator**: a small notice appears only when a section will not play in real time (effect without a GPU path, weak machine), offering to pre-render. Nothing appears when all is well.
- **Timeline skimming** also shows the frame in the viewer (ghost playhead).

---

## 6. Adjustments bar and control drawer

The bar sits above the viewer. **Only the adjustments that apply to the selection are enabled.** A dot (•) marks adjustments already active on the selected clip.

| Icon | Adjustment | Drawer controls (level 1) | "More" (level 2) | Release |
|---|---|---|---|---|
| ◐ | **Color** | Auto, exposure, contrast, saturation, temperature | Color wheels, curves, LUT | v0.1 basic, v0.3 advanced |
| ⬚ | **Crop and framing** | Fit / Fill / Crop; pan & zoom (Ken Burns) | Free rotation, numeric position | v0.1 |
| ♪ | **Volume** | Volume, fade in/out, mute | Equalizer, noise reduction, normalize | v0.1 basic |
| ⏱ | **Speed** | Slow / Normal / Fast, reverse | **Speed ramps with presets**, freeze frame | v0.2 |
| ✦ | **Effects** | Filters with preview thumbnails | Parameters, keyframes | v0.2 |
| ⧉ | **Overlay** (only on layers above the primary storyline) | Picture-in-picture, Side by side, Cutaway, Chroma key | Border, shadow, mask, blend | v0.2 |
| ⓘ | **Info** | Name, duration, source file, codec, decode path | — | v0.1 |

- The **drawer** opens between the bar and the viewer, pushing the viewer down slightly (the viewer shrinks instead of being covered).
- **"More"** expands the drawer. If the content is large (curves, keyframes), a **side panel** opens in place of the library, with "◂ Back to library".
- For titles the bar changes: **Text** (font, size, color, alignment), **Style** (presets), **Animation**.
- For transitions: **Type** and **Duration**.

---

## 7. Timeline

### 7.1 Visible structure
- **Primary storyline** (magnetic): the film's sequence. Removing a clip closes the gap; dragging pushes the neighbors.
- **Layers above** (video, titles): appear when something is dragged above the primary storyline. They stay **connected** to the primary clip underneath and move with it.
- **Audio below** (music, voiceover, effects): also appears as needed.
- **No track headers by default.** A "Show track controls" preference displays headers with mute/solo/lock/hide (level 3).
- The model is generic multitrack (`CLAUDE.md` §10); "primary + connected" is how the UI presents and edits it.

### 7.2 Elements
- **Minimap** at the top: the whole project in miniature, with the visible region highlighted (the answer to Resolve's dual timeline). Click or drag to navigate.
- **Transitions** as a small ⋈ icon **between** two clips (like Movie Maker), clickable to edit.
- **Waveform** at the bottom of every clip with audio.
- **Mouse-centered zoom** (`Ctrl+scroll`, `Ctrl+=`/`Ctrl+-`), `Shift+Z` fits the project to the width. At minimum zoom clips become equal-size thumbnails (**storyboard**, v0.2).
- **Snapping** on by default (`N` toggles).

### 7.3 Editing

With the mouse, editing works without switching tools (as in iMovie). For people coming
from Final Cut, **tools** exist and are switched by key (§8.1).

| Gesture (Select tool) | Result |
|---|---|
| Drag a clip edge | Ripple trim (magnetic) |
| Double click a junction | **Cut editor**: both sides with the unused material dimmed; adjusts the cut and the audio split (J/L-cut) |
| Drag an edge with the Trim tool (`T`) | Roll between neighbors |
| Drag the middle with the Trim tool | Slip (changes the content, keeps position and duration) |
| Click with the Blade tool (`B`) | Splits at that point |
| Position tool (`P`) | Moves without magnetism (overwrites, may leave a gap) |

The editing shortcuts are in §8.1.

---

## 8. Keyboard and command palette

### 8.1 Shortcuts (decided: iMovie/Final Cut convention)

Translation rule: **macOS `Cmd` becomes `Ctrl`**, `Option` becomes `Alt`. OmaMovie never uses
`SUPER` (reserved for Hyprland/Omarchy).

**Playback and navigation**
| Shortcut | Action |
|---|---|
| `Space` | Play/pause |
| `J` `K` `L` | Backward / pause / forward (repeat to speed up) |
| `/` | Play the selection |
| `←` `→` | Previous/next frame |
| `Shift+←` `→` | 10 frames |
| `↑` `↓` | Previous/next edit |
| `Home` / `End` | Project start / end |
| `S` | Toggle skimming |
| `N` | Toggle snapping |

**Editing**
| Shortcut | Action |
|---|---|
| `I` / `O` | Mark in/out |
| `X` | Select the whole clip under the skimmer as a range |
| `E` | Append the library selection to the end |
| `W` | Insert at the playhead |
| `Q` | Connect as a layer above, at the playhead |
| `D` | Overwrite at the playhead |
| `Ctrl+B` | Split at the playhead |
| `Delete` | Remove and close the gap |
| `Shift+Delete` | Replace with a gap |
| `Ctrl+D` | Change duration |
| `M` | Add marker |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / redo |

**Tools**
| `A` Select | `B` Blade | `T` Trim | `P` Position | `R` Range |
|---|---|---|---|---|

**Library**
| Shortcut | Action |
|---|---|
| `F` | Favorite |
| `Delete` (in the library) | Reject (does not delete the file) |
| `U` | Clear rating |
| `Ctrl+F` | Search |

**Window and project**
| Shortcut | Action |
|---|---|
| `Ctrl+=` / `Ctrl+-` / `Shift+Z` | Timeline zoom / fit the project |
| `Ctrl+1` | Show/hide the library |
| `Tab` | Cycle focus: library → viewer → timeline |
| `Ctrl+I` | Import |
| `Ctrl+E` | Export |
| `Ctrl+Shift+F` | Full-screen viewer |
| `Ctrl+K` | **Command palette** |

- Every shortcut comes from the central action system and will be remappable.
- **Check** each shortcut against the official iMovie and Final Cut Pro documentation before
  implementing it; this table was compiled from memory of the convention.
- `Ctrl+K` is the keyword editor in Final Cut; OmaMovie has no keywords, so the palette takes
  the shortcut.

### 8.2 Command palette (`Ctrl+K`)
- Lists **every** app action, searchable by name ("speed 50%", "export 1080p", "add marker").
- It is disclosure "level 3": any capability is reachable without an on-screen button.
- Matches Omarchy's style (search-driven launcher and menus).

---

## 9. Progressive disclosure

| Level | Where | Example |
|---|---|---|
| **1 — always visible** | Adjustments bar, drawer, timeline gestures | Crop, volume, fade, split |
| **2 — one click** | "More" in the drawer, side panel, cut editor | Curves, equalizer, speed ramps, keyframes |
| **3 — on demand** | Command palette, preferences, track headers | Numeric roll/slide, three-point editing, mute/solo/lock |

There are no separate "simple" and "pro" modes: a mode hides capability and forces a context switch.

---

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
- **Opaque window** (documented Hyprland rule; `omarchy-integration.md` §4).

### 10.1 Typography (decided)

- **The Omarchy font across the whole UI**, read with `omarchy-font-current` and updated live
  by the font-change hook/watcher. Fallback outside Omarchy: the system's default monospace
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
- Implementation: SVGs in `apps/omamovie/resources/icons/`, loaded through the Qt Resource
  System; tinted through the Qt Quick Controls icon mechanism (`icon.source` + `icon.color`)
  **(verify the behavior with monochrome SVGs in Qt 6.11)**.
- License: the project's own (MIT), with a credits file if any icon derives from another free set.
- The glyphs used in this document's wireframes (◐ ⬚ ♪ ...) are **placeholders only**.

---

## 11. What differs from iMovie (on purpose)

| iMovie | OmaMovie | Why |
|---|---|---|
| Limited layers, no general keyframes | Full multitrack, keyframes (v0.2), hidden until needed | Hide, do not remove |
| Mouse-oriented | Full keyboard + command palette | Omarchy's audience |
| No timeline overview | Minimap | Resolve research (dual timeline) |
| Library tied to the Photos app | Local folders + Omarchy recordings | Platform |
| A single fixed look | Omarchy theme, light/dark | Integration |
| A separate editor (FCP) for those who grow | The same app, progressive disclosure | The project's thesis |
| Horizontal formats | 16:9, 9:16, 1:1 from the new project dialog | Short-form creators (CapCut) |

---

## 12. Decisions taken (2026-10-02)

| # | Topic | Decision |
|---|---|---|
| 1 | Adjustment controls | **Drawer above the viewer** (§6) |
| 2 | Typography | **The Omarchy font** across the UI (§10.1) |
| 3 | Icons | **Our own SVG vector set** (§10.2) |
| 4 | Shortcuts | **iMovie/Final Cut convention**, `Cmd`→`Ctrl` (§8.1) |
| 5 | Start screen | **Depends on the origin**: the launcher opens Projects; a project or media file opens Edit (§3.1) |

Still open: nothing blocking. Fine-tuning will come from the prototype.

## 13. Next step

Interactive mockup (HTML, fictitious data): `Docs/mockups/ui-mockup.html`.

Next, a QML prototype within M6, with fake data, to validate sizes, density and the
half-width behavior before wiring the engine.
