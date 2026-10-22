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
