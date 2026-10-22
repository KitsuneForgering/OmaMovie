# Three-layer playback audit — 2026-10-22

**Classification:** PERFORMANCE (interactive playback). The cross-editor protocol asks for a
three-layer OmaMovie playback audit with player drops. `omamovie --m4-audit FILE SECONDS`
already opens an OmaMovie project as well as a video (`Session::open`), so the bench's layer
scene runs through it unchanged: real window on Hyprland (1920x1080 at 60 Hz), muted audio
paced by PipeWire, hardware pilot decode, 8 s from the start.

Host: i5-1135G7 / Iris Xe, Mesa 26.2.4, iHD 26.3.5, `intel_pstate` `powersave`, EPP
`balance_performance`, `balanced`, AC, load 0.4–0.8. Release build of `be7c2e5`. Scenes: the
1080p three-layer project of `tools/bench/cross-editor/layers.py` (V2/V3 halved into opposite
quadrants), the same with blur on all layers and a look and colour adjustment on V1, and both
on a 4K canvas from 4K sources ([sources](2026-10-21-gpu-input-layers-effects.md)). Three rounds,
scenes in a fixed order per round.

| Scene, 8 s at 23.976 | Composite p50 / p99 ms | Dropped video per run | A/V signed range ms | Result |
|---|---|---|---|---|
| One 1080p clip (`xed_1080p24_bt709_noise-v3.mp4`) | 2.4 / — | 0, 0, 1 | — | PASS ×3 |
| 1080p three layers | 2.3–2.4 / 6.1–7.3 | 2, 2, 2 | −38..+21 | PASS ×3 |
| 1080p three layers + effects | 1.6–3.2 / 5.8–7.8 | 1, 1, 2 | −39..+21 | PASS ×3 |
| 4K three layers | 4.2 / 10.0–13.0 | 2, 1, 2 | −36..+21 | PASS ×3 |
| 4K three layers + effects | 4.1–4.2 / 11.9–13.0 | 3, 2, 2 | −37..+21 | PASS ×3 |

No audio underruns; about 190 composites per run. Composition stays far below the 41.7 ms
frame period even at 4K with effects, yet every layered scene drops one to three frames per
8 s where the single clip drops zero or one. The extra drops therefore do not come from
compositing cost.

**Attribution (same day).** `VideoScheduler` now also counts frames the producer queued
already due (`late`) and frames it never prepared because the clock had passed them
(`skipped`, which `dropped` never included); the audit prints both. Three more rounds: the
single clip dropped 0/1/1 (late 2/1/1, skipped 0), the 1080p layers 1/2/2 (late 1, skipped 0),
the 4K layers with effects 1/2/2 (late 1, skipped 0). The one late frame is the first, due as
playback starts. So the producer keeps up with three decoders even at 4K; the drops are
on-time frames overtaken between two takes on the render thread, whose composite interval
p99 reaches 60–130 ms on the single clip too. The decoder hypothesis is refuted.

**Where the drops fall.** A temporary log of each drop's target frame (not committed) put every
one of them, on all three scenes and three rounds, at frame 1 or 2: as playback starts, while
the first frames of three freshly positioned decoders are composed, the clock overtakes one
or two of them. None fell later. The audit now reports the count after its 2 s settle beside
the total, as it already did for audio underruns: six more runs read `after 2 s 0` on the
single clip, the 1080p layers and the 4K layers with effects. So in steady state the layered
scenes drop no frames on this host; what the layers cost is a one- or two-frame stutter at
the start of playback, which a preroll (waiting for the first frames before starting the
clock) could remove. Not attempted: it trades a little start latency. The interval percentiles
(p50 40, p95 60 ms) follow the audit's 20 ms UI timer, not presentation.

Not comparable with melt: its `sdl2` consumer reports no dropped frames (see the
[summary](../../analysis/omamovie-2026-10-20-cross-editor-summary.md)). Not measured: longer
runs, seek, other vendors.

**Consequence for the adaptive preview (same day).** The runs above used this host's settings
(`[preview] quality=full`). With a fresh configuration, where the automatic preview is the
default, three audits of `a2160.mp4` all logged `preview at half resolution` (3, 5 and 9 of
about 125 frames dropped), each for the rest of the playback, though none of those drops fell
after the 2 s settle: the start transient alone halved every 4K playback. The rule now lives in
`PreviewAdapt` and leaves the first 12 shown frames of each playback out of its window; three
more fresh-configuration audits stayed at full resolution (2, 2 and 5 start drops, 0 after the
settle). Sustained drops still halve the preview (`tests/app/test_preview_adapt.cpp`).
