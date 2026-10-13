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
- The 1500 px docked/overlay breakpoint has a 100 px hysteresis band during window resizing:
  enter the docked layout above 1550 px, return to the overlay layout below 1450 px.
  This avoids repeated full-layout switches when a tiled window jitters near the breakpoint.
- Adjustments bar labels become icons only (with a tooltip and shortcut).

[← UI design](ui-design.md)
