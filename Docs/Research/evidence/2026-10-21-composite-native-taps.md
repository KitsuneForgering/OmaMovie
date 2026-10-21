# Layer pass: one tap on the source grid, no target read when opaque — 2026-10-21

**Classification:** PERFORMANCE; outputs byte-identical. Host i5-1135G7 / Iris Xe, Mesa iHD
26.3.5, FFmpeg 9.0.2, `powersave`/`balance_performance`, AC. Release build of `08e0c07` (A)
against the same with this shader change (B).

**Hypothesis, from measurement.** On the 4K23.976 export (`a2160.mp4`, GPU-native input) the
render engine was busy about 86% of wall time and the render thread's time per frame matched
GPU time, so the export was GPU-bound and removing the per-frame fence wait could recover at
most the gap. Inside the layer pass, bilinear filtering runs `detailed()` (YUV fetch → RGB →
linear) on four taps per output pixel; on a native-size layer the sample falls on the source
grid (`t == 0`) and three taps weigh zero. Two changes in `composite.comp`:

1. `t == 0`: compute only the first tap (`mix(x, y, 0)` is `x`, so the result is the same).
2. A fully opaque pixel in normal blend stores the source without reading the target.

A rejected third idea: skipping the target clear. Measured by switching it off: about 0.1 ms
at 4K (the driver's fast clear), not worth a coverage test.

**Microbenchmark** (`compositor_tests` "measures each effect pass", 20 warmed renders, a 1080p
H.264 source filling the output; five or six runs each, load 2-5):

| Case | A | change 2 only | both (B) |
|---|---|---|---|
| 1080p, native size | 2.21–2.36 ms | 2.22–2.54 ms | 1.00–1.16 ms |
| 4K, upscaled 2× | 7.39–7.53 ms | 7.15–7.33 ms | 7.23–7.51 ms |

A first form of change 1 (whole bilinear block in the `else`) cost the upscaled case about
0.3 ms; computing the first tap before the branch removed that.

**Whole export** (`export_bench`, `measure.py`, A and B alternated per round, load 1.1-1.9;
`gpu_render_s` is the DRM render engine's busy time):

| Fixture, route | Rounds | A wall s, median (range) | B wall s, median (range) | A / B render engine s |
|---|---|---|---|---|
| 4K, GPU-native input | 7 | 3.50 (3.46–4.81) | 2.77 (2.73–3.81) | 2.78 / 1.96 |
| 4K, NV12 readback | 3 | 3.72 (3.72–3.82) | 3.37 (3.32–3.37) | 2.97 / 2.77 |
| 1080p, GPU-native input | 3 | 2.99 (2.98–3.03) | 2.22 (2.22–2.27) | 2.33 / 1.83 |
| 1080p, NV12 readback | 3 | 3.02 (3.02–3.07) | 2.59 (2.58–2.66) | 2.47 / 2.09 |

The first 4K round of each binary ran at load 1.4 and is the slow outlier in both ranges.
Every pair of movies is byte-identical (`cmp`). On the readback route user CPU rose from about
2.2 to 2.5 s per run (more work per second of the same CPU-side stages); not investigated.

**Scope.** Native-size layers (source size equal to the canvas) gain; scaled, rotated or
cropped-off-grid layers take the four-tap path as before. Playback composites the same pass, but
playback was not measured. Single host; no other GPU vendor.
