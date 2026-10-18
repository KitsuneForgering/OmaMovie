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

[Return to ui-ux-evaluation-2026-10-10.md](ui-ux-evaluation-2026-10-10.md)
