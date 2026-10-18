## Current interaction model

| Task | Visible path | Advanced path |
|---|---|---|
| Find media | Library and import button | Source viewer with range marks |
| Arrange | Drag from the library; magnetic storyline | Insert, overwrite, connected lanes, snapping and timeline zoom |
| Adjust a clip | Its context menu opens one adjustment drawer | Numeric values, keyframes and deeper controls in each drawer |
| Inspect a cut | Transition marker at the junction | Type and duration in the cut menu |
| Find a command | Context menu or shortcut | `Ctrl+K`, fuzzy search, `:command` notation and shortcut remapping |
| Export | Export action in the top bar | Current export dialog and progress UI |

The UI keeps the viewer free of permanent inspectors. At narrow widths the library becomes a
temporary overlay. At wide widths it can be hidden or resized by dragging the divider or using
Left/Right while that divider has keyboard focus; the upper area and timeline have a draggable
divider. The menu is selected by target: storyline, overlay, audio lane, title, transition,
library item or empty timeline. Clip adjustments form a one-level tree: Picture for video,
Sound only when the clip has audio, and Speed only for video. Split, connection, detach and deletion stay
at the root. The Sound branch reuses the Volume drawer's detailed EQ/noise controls.
The title and cut menus have no picture or sound branch.

[Return to ui-ux-evaluation-2026-10-10.md](ui-ux-evaluation-2026-10-10.md)
