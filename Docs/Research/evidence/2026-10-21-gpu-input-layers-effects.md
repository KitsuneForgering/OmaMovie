# GPU-native encoder input on layered and effect scenes — 2026-10-21

**Classification:** PERFORMANCE + CORRECTNESS. Same host, build and project per table; one
variable (`export_bench … gpu-input`). Answers ADR-0023's open check on multi-layer and
effect-heavy fixtures; it does not accept the ADR or change the default.

Host: i5-1135G7 / Iris Xe, Mesa 26.2.4, iHD 26.3.5, FFmpeg n9.0.2, `intel_pstate`
`powersave`, EPP `balance_performance`, `balanced` profile, AC, load average 0.2–0.8.
Release build of `67b50f2`, `build/release/tools/export_bench`, VA-API H.264, hardware decode.

**Scenes.** *Layers*: `tools/bench/cross-editor/layers.py` over its three 1080p23.976 BT.709
sources (`layer0-2.mp4`, sha256 `f151d503…`, `290d194e…`, `130b979d…`), 240 frames, V2 and V3
halved into opposite quadrants. *Layers + effects*: the same project with `oma.detail` at
−0.6 (blur) on all three layers, and `oma.look.vintage` 0.7 plus exposure 0.3, contrast 0.2,
saturation 0.1 on V1 (its movie differs from the plain scene's, 17.4 dB PSNR).

**A** = readback (default): `read_nv12`, VA-API upload from the readback rows. **B** =
`gpu-input`: each frame written into the encoder's own surface. Five rounds, the order
reversed every other round. Medians with [min–max].

| Scene | Wall A ms | Wall B ms | CPU/frame A → B ms | Render A → B ms | Encode stage A → B ms |
|---|---|---|---|---|---|
| Layers | 1649 [1625–1707] | 1496 [1460–1542] | 5.10 → 2.70 | 4.86 → 3.97 | 2.71 → 0.90 |
| Layers + effects | 2262 [2219–2273] | 2022 [2017–2037] | 6.30 → 3.40 | 7.34 → 6.02 | 3.25 → 0.94 |

B used its surfaces for all 240 frames in every run. The last round's A and B movies are
byte-identical (`cmp`) for both scenes. Wall time fell 9.3% and 10.6% with non-overlapping
ranges, and process CPU per frame by 47% and 46%; render time also fell because the readback
conversion and fence wait leave the render stage. Not measured: GPU engine busy time and
memory, 4K layered scenes, 60 fps sources with effects, other vendors.

## 4K layered scenes, with GPU memory — 2026-10-22

Same method at `bb95894`, plus `tools/bench/cross-editor/measure.py` for process CPU, peak RSS,
peak GPU-resident memory and DRM engine busy time. Sources: the same three generators at
3840x2160 23.976, 10 s (`~/.cache/omamovie-bench/fixtures/layers4k`, sha256 `2f8db9ff…`,
`d8263118…`, `16590f88…`), `layers.py`'s project on a 4K canvas with V2/V3 offset ±960/±540;
the effect variant as above. Four alternating rounds, load 1.5–4 at the start.

| Scene (4K, 240 frames) | Wall A s | Wall B s | CPU s A → B | Peak RSS MB A → B | GPU-resident MB A → B | Video engine busy s A → B |
|---|---|---|---|---|---|---|
| Layers | 5.78 [5.76–5.86] | 5.34 [5.25–5.36] | 3.46 → 1.33 | 235 → 203 | 1073 → 1082 | 5.89 → 5.49 |
| Layers + effects | 6.79 [6.75–6.84] | 6.12 [6.11–6.28] | 3.62 → 1.31 | 224 → 203 | 1102 → 1114 | 5.63 → 5.41 |

B wrote all 240 frames into encoder surfaces; A and B movies byte-identical for both scenes.
Wall time fell 7.6% and 9.9% (ranges apart), CPU 62–64%; GPU-resident memory rose about
10 MB (1%). The video engine (decode and encode) is busy for about the whole export on both
routes, so at 4K the export is bound there, not by the hand-off. Still not measured: 60 fps
sources with effects, other vendors.

## 1080p60 with effects — 2026-10-22

`xed_1080p60_noise.mp4` (600 frames, sha256 `779347a2…`) on V1 with `oma.detail` −0.6,
`oma.look.vintage` 0.7 and the colour adjustment above, and the same clip halved into the
top-left quadrant on V2 (a 60/1 sequence from a two-track OTIO, `oma-project convert`).
`measure.py`, five alternating rounds at `0726cd1`, load 0.4–1.0.

| Route | Wall s | CPU s | Peak RSS MB | GPU-resident MB | Render busy s | Video busy s |
|---|---|---|---|---|---|---|
| A readback | 3.49 [3.42–3.62] | 2.40 | 139 | 180 | 2.86 | 2.03 |
| B GPU-native input | 3.54 [3.50–3.57] | 1.57 | 131 | 184 | 2.77 | 1.75 |

600/600 frames in encoder surfaces; movies byte-identical. Wall time is a tie with overlapping
ranges: the render engine is busy about 80% of the export, so this scene is bound by
compositing and effects. B still uses 35% less CPU and 4 MB more GPU memory. With this, the
layered, effect, 4K and 60 fps checks of ADR-0023 have no case where B is slower beyond noise
or differs in output; other vendors and the surface-failure fallback test remain open.
