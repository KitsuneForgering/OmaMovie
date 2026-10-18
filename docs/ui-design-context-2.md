# Historical shell state (part 1)

Shell state (2026-10-03), aligned with this document:
- Top bar per §2.1 (back, name, undo/redo, import, export); undo/redo name the
  command they revert ("Undo Split"); export stays disabled until M7.
- Adjustments per §6 (the permanent bar was removed on 2026-10-04: they open from the clip's
  context menu); Color, Crop, Effects (video clips),
  Volume (clips with audio) and Info are enabled; Speed and Overlay show their release
  in the tooltip. A dot marks what is adjusted on the selected clip. Labels collapse
  to icons below 1500 px (§2.2).
- Color: exposure (±2 EV), contrast, saturation and temperature, in linear light
  (no "Auto" yet). Crop: Fit/Fill/Stretch and the four edges; "More" adds position,
  scale and rotation, with keyframes (2026-10-04): "Add key" keeps the framing at the
  playhead, and once a clip has keys the sliders set the key at the playhead (adding one);
  "Ken Burns" pushes in to 120% over the clip, eased. No key markers on the clip and no
  on-viewer handles yet.
  Effects: one filter per clip (Black & White, Sepia, Vintage, Cool, Warm, Vignette)
  picked from previews of the clip itself, its amount, and a Soften/Sharpen control.
  Color "More" (ADR-0012): tabs for lift/gamma/gain wheels (drag the tint, slider for the
  level, double-click resets), curves (master, red, green, blue; click adds, drag moves,
  double-click removes a point) and a LUT (load a `.cube`, choose among the project's LUTs,
  amount). Reset clears the grade too.
- Transitions (ADR-0011): a ⋈ marker on every cut between touching storyline clips opens
  None / Cross dissolve / Dip to black / Wipe and 0.5, 1 or 2 s; `Ctrl+T` adds a 1 s cross
  dissolve at the cut nearest the playhead. The span it actually plays is drawn over the cut
  (shorter when the clips have little media to spare; a notice says when it stays a plain
  cut). Sound crossfades over the same span.
- Transport per §5 (timecode `HH:MM:SS:FF`, start/end, previous/next frame,
  reverse, stop/reset, play, forward, full-screen viewer); `K` still pauses in place.
  Failures appear over the viewer,
  routine status does not.
- Timeline per §7.2/§7.3: no edit bar (edits come from context menus and shortcuts);
  minimap with the visible region and playhead, clips at
  their sequence time (gaps show), a playhead that clicks and drags scrub, edge
  drags that ripple-trim, wheel scrolling, Ctrl+wheel zoom around the pointer,
  keyboard zoom, fit; no track headers or edit buttons. Snapping (2026-10-04, on by
  default, `N` or the magnet button toggles): lane clip moves and edge trims snap within
  8 px to clip edges on any track, the playhead and zero, with a line on the edge.

[← UI design](ui-design.md)
