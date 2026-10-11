# UI/UX evaluation — 2026-10-10

The product rule is **simple interface, simple mental model, complex backend**. The editor's
default view has a library, a viewer and a timeline. An action should appear on its target,
through a shortcut, or in the command palette without acquiring another implementation.

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

## Verification

The GUI smoke drives mouse edits, keyboard menus, palette search and execution, undo/redo,
contextual title, picture and sound branches, a library resize, and layout geometry at 820 and
640 px. `make test`
checks the headless editing and media layers. The smoke's `grabWindow()` image is useful for
rough visual review, but its Qt/Vulkan capture path can disagree with the measured item geometry
and it does not establish screen reader or real user task performance.

## Context tree and Omarchy check — 2026-10-11

The context tree pilot uses native Qt menus with one submenu level. A video clip with sound
shows Picture, Sound and Speed; the audio lane shows Sound; a title shows Edit text;
a cut shows transition type and duration. Split, detach, connection and deletion stay at the
root. The GUI smoke opens these menus with keyboard and pointer, checks the selected clip,
and invokes Color through the Picture branch. All entries reuse the existing actions or
editing commands. Its full run and the 10 headless test groups passed on 2026-10-11.

The next pass separates basic and precise controls within those same shallow branches.
Picture has one menu entry for each drawer: Framing and transform, Color and grading,
and Effects. Sound
offers levels/fades, mute, equalizer, noise reduction and clip peak normalization. The
advanced entries open the appropriate section of an existing drawer and place keyboard
focus on a usable control. They are registered actions, so the palette and menus reach
the same implementation. The GUI smoke checks the selected video clip, the target drawer
and focused control for transform and grading; it repeats the check for EQ and noise
reduction on an audio lane. It passed on 2026-10-11. Clip peak normalization is distinct
from the proposed full-mix loudness analysis in the M8 plan.

A domain audit removed duplicate Picture menu entries that opened the same drawer.
Audio-only clips and titles no longer show Speed: the current retiming pipeline silences
audio outside normal speed, and titles have no useful media retiming path. The 2× command
is also unavailable for those selections in the palette. Add title moved from a library
item's menu to the empty timeline, where the new title is inserted. The GUI smoke checks
the shorter Picture, Sound and title menus and still reaches Color by keyboard.

The installed Omarchy theme was **Sword Art Omarchy**, with JetBrainsMono Nerd Font. On
Hyprland 0.56.2, the normal editor window reported class `omamovie`, `xwayland: false` and
`floating: false`. Beside a terminal it measured 943 × 1028 px. Resizing the tiled editor
with Omarchy's `hl.dsp.window.resize` dispatcher gave 643 × 1028 px without floating it.
At that width the root and Sound submenu remained inside the client, the viewer stayed
visible, and the root action labels were readable. The original 200 px menu truncated
“Delete and close the gap”; the pilot now uses a 280 px root and up to 320 px for Sound
and Speed, capped by the window width. No Hyprland or Omarchy user configuration changed.

This is a technical check, not a comparison of how quickly people find adjustments in the
flat and grouped menus. The Omarchy default opacity tag still applies to the window; color
critical preview opacity needs its own compositor-rule decision and validation.

## Next design and engineering gates

1. **Direct manipulation:** framing already supports moving, scaling, rotating and crop edges.
   Add on-canvas text editing, then evaluate masks and tracking only with a supported backend.
2. **Command coverage:** continue moving existing menu and drawer operations into registered
   actions. A future agent interface should call the same validated editing commands through an
   explicit API; it should not automate UI widgets.
3. **Navigation:** test a short edit with mouse and keyboard at 640, 820 and 1600 px, including
   larger text and a light Omarchy theme. Keep the existing remappable shortcuts; evaluate
   optional keymap profiles only after checking conflicts with text fields and Hyprland.
4. **Professional features:** roll, slip, slide, markers, nested sequences, masks, tracking,
   scopes and broader export controls need model and pipeline support before presenting them as
   working menu items. Group new effects by intent and reveal precision controls in the relevant
   drawer.
5. **Accessibility:** verify a screen reader, keyboard focus in every popup and drawer, larger
   text, contrast and target sizes. The current smoke covers focus paths, not screen readers.

Keep unavailable features out of active menus. A command may be discoverable only when the
selected object and the backend can execute it. This preserves professional capacity as the
backend grows without promising a control that does nothing.

## Resize and screen-switch follow-up — 2026-10-11

The reported symptom is transient flicker inside the OmaMovie window during repeated
resizes or screen switches. The GUI smoke now cycles 24 window sizes, Projects/Editor and
Settings, then jitters across the 1500 px layout breakpoint. It checks that the viewer and
timeline recover and that the layout does not flip back and forth inside the hysteresis band.
The run passed on the Intel/Wayland target. The timeline now coalesces its two visible-range
notifications into one model sync per event loop, and the library layout keeps its current
mode within 1450–1550 px. A separate minimap paint error was found: the connected-clip
outline used undefined coordinates when repainting, and it now uses minimap coordinates
and repaints when the followed clip changes. These changes remove concrete sources of
intermediate repaint and layout churn; the smoke verifies the final rendered state, not every
frame visible to the compositor during a manual resize.
