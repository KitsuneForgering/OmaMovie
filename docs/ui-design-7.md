## 7. Timeline

### 7.1 Visible structure
- **Primary storyline** (magnetic): the film's sequence. Removing a clip closes the gap; dragging pushes the neighbors.
- **Layers above** (video, titles): appear when something is dragged above the primary storyline. They stay **connected** to the primary clip underneath and move with it.
- **Audio below** (music, voiceover, effects): also appears as needed.
- **No track headers by default.** A "Show track controls" preference displays headers with mute/solo/lock/hide (level 3).
- The model is generic multitrack (`CLAUDE.md` §10); "primary + connected" is how the UI presents and edits it.

### 7.2 Elements
- **No edit bar** (removed 2026-10-04: it duplicated the context menus). Append, insert and
  overwrite are in the library item's context menu; split, delete, lift and detach audio in the
  clip's; transitions on the ⋈ marker. Every one keeps its shortcut (§8.1).
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
| Drag a storyline clip | Moves it to the nearest cut; its gap closes and the neighbors make room (magnetic) |
| Drag from the library | Pictures insert at the nearest storyline cut; sound lands on the lane under the pointer |
| Right click a clip or ⋈ | One-level context tree: Picture has one entry per drawer (Framing and transform, Color and grading, Effects) for video; Sound offers levels/fades, mute, EQ, noise reduction, and clip peak normalization only when audio exists. Speed is offered on video clips, where retiming affects the picture; audio-only clips and titles have no Speed branch because their sound/time behavior is not implemented. Split, detach, connection and delete remain at the root. Titles have Edit text; cuts show only transition type and duration. Precise transform, grading, EQ and noise controls remain in the drawers and command palette. Add title belongs to the empty timeline menu, not a library item's menu. |
| Double click a junction | **Cut editor**: both sides with the unused material dimmed; adjusts the cut and the audio split (J/L-cut) |
| Drag an edge with the Trim tool (`T`) | Roll between neighbors |
| Drag the middle with the Trim tool | Slip (changes the content, keeps position and duration) |
| Click with the Blade tool (`B`) | Splits at that point |
| Position tool (`P`) | Moves without magnetism (overwrites, may leave a gap) |

The editing shortcuts are in §8.1.

---

[← UI design](ui-design.md)
