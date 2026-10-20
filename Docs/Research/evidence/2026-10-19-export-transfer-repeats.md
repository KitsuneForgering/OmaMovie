# Export transfer repeats and GPU use — 2026-10-19

**Classification:** PERFORMANCE / M12 baseline. Repetitions and GPU use for the
[2026-10-16 phase split](2026-10-16-export-transfer-phases.md); not a speedup claim.

`tools/bench/export-transfer/run.sh DIR 3` on the Release build of `19bc873` (clean tree),
then `run.sh DIR 1` after `measure.py` gained DRM fdinfo sampling. Host: i5-1135G7 / Iris
Xe (i915), Mesa iHD 26.3.5, FFmpeg 9.0.2, `intel_pstate` `powersave` with EPP
`balance_performance`, `balanced` profile, AC. Load 1-min average rose from 2.8 to 7.7
during the three-repeat run, mostly from the exports themselves (about five cores busy at 4K).

| 3 repeats (min / median / max) | 1080p60, 60 frames | 4K23.976, 240 frames |
|---|---:|---:|
| Export duration (ms) | 673 / 745 / 756 | 8576 / 9499 / 12860 |
| Process CPU per frame (ms) | 23.5 / 27.1 / 29.0 | 184 / 272 / 277 |
| Peak RSS (MB) | 322 / 327 / 376 | 636 / 700 / 702 |
| Per-frame build (decode) (ms) | 0.42 / 0.53 / 0.55 | 3.2 / 11.5 / 22.8 |
| Per-frame render + readback (ms) | 10.0 / 10.9 / 11.4 | 27.6 / 30.2 / 32.0 |
| VA-API upload total (ms) | 187 / 213 / 238 | 2549 / 4112 / 5500 |

Every run used software decode, Vulkan composition and VA-API H.264 encode; frame counts
matched (60 and 240), paired PSNR was identical in every repeat (40.97 dB and 25.76 dB, the
latter a lossy re-encode of grainy `testsrc2`, as before), and the tone export's audio
`apsnr` was 170.136 dB on both channels.

**Spread.** 4K render + readback stayed within ±9% of its median, while decode/build (7×) and VA-API
upload (2.2×) drove the 1.5× duration spread, together with more CPU time for the same
work. Thermal or frequency state was not recorded per run, so the cause is unattributed.
Three repeats do not give stable percentiles; compare only medians of same-load runs.

**GPU use (one run with sampling).** DRM fdinfo of the app process, counted once per DRM
client, sampled every 50 ms:

| | 1080p60 | 4K23.976 | 1080p tone (479 frames) |
|---|---:|---:|---:|
| Wall (s) | 1.62 | 8.83 | 7.32 |
| Render engine busy (s) | 0.41 | 4.94 | 3.10 |
| Video engine busy (s) | 0.14 | 1.75 | 0.91 |
| Peak GPU-resident memory (MB) | 265 | 605 | 273 |
| User CPU (s) | 2.08 | 44.18 | 35.51 |

At 4K the render engine was busy about 56% of wall time and the video engine about 20%,
while the process used about five CPU cores. The GPU is not saturated; this export is
mostly CPU-bound in the staged path. A plausible but untested reading: software 4K decode
and the explicit row copies/upload dominate CPU time. Test hardware decode for export and
the M12 GPU-native encoder input separately against these medians before attributing gains.
fdinfo misses up to 50 ms of work at exit and counts driver memory of the whole process.
