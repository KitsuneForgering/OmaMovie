# UI design — OmaMovie

The [2026-10-10 UI/UX evaluation](ui-ux-evaluation-2026-10-10.md) records the current
implementation, interaction checks and remaining gates against the product direction.

> Product choices from 2026-10-02; evidence reviewed on 2026-10-03, with the decisions in §12. Based on `Docs/Research/imovie.md`
> (principles), `movie-maker.md`, `capcut.md`, `davinci-resolve.md` and
> `omarchy-integration.md`. Implementation rules remain in `CLAUDE.md` §11.
>
> Inspired by iMovie **in its principles**, without copying Apple's visual identity, icons,
> feature names or trade dress.

The choices below remain product preferences. Competitor manuals establish interaction
examples, not effectiveness for Omarchy users. [Research audit](Research/skeptical-review.md)
sets M6 comparison criteria. Width thresholds, default proportions, storyboard projection
and font density are prototype hypotheses; test different fonts, scaling and keyboard focus.
Engine performance, color and platform integration are not validated by the HTML mockup.

The 2026-10-03 `make run-gui` window was an S4/M4 editor-shell prototype. The following status paragraphs are historical; use the implementation plan for current capabilities. It opens on
Projects, enters Edit for an imported video, and implements the library/viewer/
timeline layout below with a responsive library overlay at half width. The
storyline is the M5 timeline model (`Session` in `apps/omamovie/src/` owns an
`oma::timeline::Editor`): append, insert and overwrite from the library, split at
the playhead, ripple delete or replace with a gap, ripple trim by dragging clip
edges, and undo/redo, each one history entry. The viewer shows the sequence at the
playhead, composited on the GPU by the Vulkan compositor with each clip's fit,
crop, transform and opacity and handed to Qt without a readback (decoding is
still software), and the sequence's audio plays through PipeWire;
the frame shown follows the audio actually heard (frames are dropped, never the
audio stretched). The Volume drawer (§6) sets volume (in dB), fades and mute per clip,
one command per committed change, and fades show as ramps on the clip; its "More" level
adds a three-band equalizer with presets, noise reduction (strength over a noise level
measured from the clip's quietest tenth) and normalize (loudest peak to −1 dBFS; not
loudness matching). Sound files import to **audio lanes** below the storyline (§7.1):
append/insert/overwrite place them there, clips drag between lanes and in time, edges
trim without ripple, and a lane disappears with its last clip. **Detach audio**
(`Ctrl+Alt+S`) moves a storyline clip's sound to a lane for J- and L-cuts. Clips with
sound show their waveform. There is no project persistence, connected video layer,
snapping or link between a lane clip and the storyline clip above it (lanes do not
follow storyline ripples) yet. The Omarchy palette and font are read at startup; live
theme updates remain M6 work.

[← UI design](ui-design.md)
