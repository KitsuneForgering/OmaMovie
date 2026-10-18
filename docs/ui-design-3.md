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

[← UI design](ui-design.md)
