# Whole-export A/B: GPU-native encoder input against the readback — 2026-10-21

**Classification:** PERFORMANCE + CORRECTNESS. Same host, build, fixture and settings; one
variable. Host: i5-1135G7 / Iris Xe, Mesa 26.2.4, iHD 26.3.5, FFmpeg n9.0.2, `intel_pstate`
`powersave`, EPP `balance_performance`, `balanced` profile, AC; load average 0.8 at the start.
Release build of `d25bd73` plus the pilot switch. Fixture `a2160.mp4` (4K23.976 H.264,
240 frames, the [2026-10-19 A/B](2026-10-19-export-hardware-decode.md)'s), `--export-audit`,
VA-API H.264 encoder, hardware decode (Auto), fresh config and cache per run,
`tools/bench/cross-editor/measure.py`; the two routes alternate within each round.

**A** = readback (default): `read_nv12`, host row copies, VA-API upload. **B** =
`OMA_EXPORT_GPU_INPUT=1`: each frame written by `write_nv12` into the encoder's own surface
(ADR-0023, proposed).

| Run | A wall s | A user CPU s | B wall s | B user CPU s |
|---:|---:|---:|---:|---:|
| 1 | 9.11 | 13.05 | 7.04 | 9.89 |
| 2 | 7.96 | 12.23 | 6.98 | 9.92 |
| 3 | 8.07 | 12.42 | 7.00 | 9.86 |
| median | **8.07** | **12.42** | **7.00** | **9.89** |

Wall time is the whole process (start-up, project, export, exit). B's three runs are within
0.06 s of each other; A's spread was 1.15 s. **Correctness:** B's audit reports 240 frames written
into encoder surfaces, A's 0; the last round's two movies are byte-identical (`cmp`), as
`tests/app/test_export_gpu_input.cpp` requires on its fixture.

**Second fixture, same day and method:** `a1080.mp4` (1080p23.976 H.264, 479 frames), load
average 0.5 at the start.

| Run | A wall s | A user CPU s | B wall s | B user CPU s |
|---:|---:|---:|---:|---:|
| 1 | 4.95 | 7.38 | 4.40 | 5.91 |
| 2 | 4.99 | 7.42 | 4.50 | 5.97 |
| 3 | 4.97 | 7.37 | 4.35 | 5.80 |
| median | **4.97** | **7.38** | **4.40** | **5.91** |

479/479 frames written into encoder surfaces in B; the last round's movies byte-identical.

**Reading:** the GPU-native input saved about 13% (4K) and 11% (1080p) of wall time and 20% of
user CPU on both. That is less than the 7-8 ms per frame the S9 spike measured for the
hand-off alone (240 frames x 7.5 ms = 1.8 s), consistent with the readback having partly
overlapped encoding on the encoder stage. Three rounds give a range, not a confidence interval.
Not measured: GPU engine busy time and memory for B, 60 fps and HDR sources, effects.
