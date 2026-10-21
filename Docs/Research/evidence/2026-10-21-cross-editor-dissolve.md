# Dissolve scene against melt, and the import's centring roll — 2026-10-21

**Defect found by building the scene.** OmaMovie centres every dissolve on its cut. melt's mix
lies wholly before its cut (an EDL dissolve wholly after it), and `project::from_ir` centred it
without moving the cut, so the mixed frames came half a dissolve late: on the 24-frame scene
below, output frames 240-263 instead of melt's 228-251. The import now rolls the cut to the
dissolve's middle (`centring_roll`, `libs/project/src/from_ir_cuts.cpp`) and reports it; without
incoming media before the new cut it centres on the old cut and says so
(`tests/project/test_from_ir_offcentre.cpp`: MLT, EDL, centred and no-media cases; the first,
second and fourth fail with the roll disabled).

**Scene.** `run.sh 3 … dissolve` (`OMA_BENCH_HEADLESS=1`): the BT.709 bench source, frames 0-251
and 288-478 with a 24-frame luma mix (`melt … -mix 24 -mixer luma`), 419 frames; OmaMovie imports
the document melt writes for it. `assert_output.py dissolve` compares every frame with FFmpeg's
`xfade` of the same source frames: at least 35 dB luma PSNR outside the mix and 30 dB inside it
(OmaMovie mixes in linear light, ADR-0006; xfade and melt on coded values). Audio is checked at
six points away from the crossfade. A file with the mix 12 frames late fails (30.2 dB outside).

**Alignment.** Against xfade at offsets 216/227/228/229/240 over output frames 200-279, OmaMovie's
mean luma PSNR was 30.3/32.8/**41.0**/33.0/32.8 dB: the mix lands on melt's frames, not one off.
Directly against melt's movie, mixed frames 228-251 score 34-37 dB, their neighbours 41 dB.

**Run** (OmaMovie `49c2348` plus this change, Release; melt 7.42.0, Kdenlive 26.08.2; i5-1135G7 /
Iris Xe, iHD 26.3.5, FFmpeg 9.0.2, `powersave`/`balance_performance`, AC). The machine was **not
idle**: a browser and another project's benchmark ran, load average 9.9-12.2, so wall times are
noisy; CPU seconds are steadier. All 12 exports passed ([raw report](2026-10-21-cross-editor-headless-dissolve.txt)).

| Route | OmaMovie wall s, median (range) | melt wall s, median (range) | user CPU s, OmaMovie / melt |
|---|---|---|---|
| VA-API H.264 | 4.54 (3.76–6.25) | 7.74 (7.09–13.96) | 2.6 / 21.1 |
| libx264 | 15.52 (15.07–17.64) | 19.55 (19.48–27.09) | 81.6 / 103.5 |

Worst luma PSNR outside the mix: OmaMovie 44.32 / 41.08 dB, melt 45.03 / 41.27 dB (VA-API /
libx264); outputs byte-identical across rounds per editor and route. melt's stereo is 0.69 of the
mono source (its equal-power upmix), OmaMovie's 0.98, as in the other scenes.

**MLT round trip rerun** (`roundtrip.sh`, headless, 12-frame mix, libx264): outside the mix 113
frames, worst 39.7 dB (unchanged); inside it 19 frames, worst **33.4 dB**, against 27.3 dB on
2026-10-15 ([earlier run](2026-10-15-mlt-roundtrip.txt)), where the frames were six late.

Not measured: an idle-machine repeat; Kdenlive's own `qtblend`/movit compositing; other
transitions (wipes stay reported hard cuts). Three loaded rounds give ranges, not a speed claim.
