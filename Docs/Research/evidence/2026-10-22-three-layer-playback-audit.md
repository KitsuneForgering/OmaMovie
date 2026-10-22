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
compositing cost; three concurrent decoders or their scheduling are the next hypothesis, not
yet measured (the audit does not attribute a drop to a stage). The interval percentiles
(p50 40, p95 60 ms) follow the audit's 20 ms UI timer, not presentation.

Not comparable with melt: its `sdl2` consumer reports no dropped frames (see the
[summary](../../analysis/omamovie-2026-10-20-cross-editor-summary.md)). Not measured: longer
runs, seek, other vendors.
