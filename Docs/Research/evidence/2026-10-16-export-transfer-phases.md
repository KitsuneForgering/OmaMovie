# Export transfer phases — 2026-10-16

**Classification:** PERFORMANCE / QA GAP. This is a one-run diagnostic, not a speedup claim.
The previous counter in `2026-10-15-export-transfer-baseline.md` included NV12 conversion,
GPU synchronization, and host copying in one readback duration. The export path also did not
report the actual compositor downgrade reason or software decode output volume.

`tools/bench/export-transfer/run.sh /tmp/omamovie-export-audit-v01-real 1` ran in the
graphical Intel session using the Release build from working tree `9f75558` plus this change.
The CPU is an i5-1135G7 with Iris Xe; FFmpeg is 9.0.1. The sandbox cannot initialize this
graphical path. Run logs and output movies are in `/tmp/omamovie-export-audit-v01-real/`.

| Measured boundary | 1080p60, 60 frames | 4K23.976, 240 frames |
|---|---:|---:|
| Export duration | 771 ms | 8850 ms |
| Process CPU during export | 1613 ms | 44798 ms |
| Software decode output | 187 MB | 2986 MB |
| Software rows copied into Vulkan staging | 187 MB / 55 ms | 2986 MB / 900 ms |
| GPU NV12 dispatch (Vulkan timestamps) | 33 ms | 578 ms |
| NV12 fence wait (includes queued work) | 123 ms | 860 ms |
| NV12 mapped buffer copied to host vector | 17 ms | 367 ms |
| CPU NV12 rows copied into writer AVFrame | 187 MB / 49 ms | 2986 MB / 679 ms |
| CPU AVFrame uploaded to VA-API surface | 187 MB / 190 ms | 2986 MB / 2686 ms |

Both exports used software decode, Vulkan composition, and VA-API H.264 encode. All GPU
readback frames had a valid Vulkan timestamp. These are overlapping phase totals, so they
must not be added to infer export wall time. The counters are explicit application API
boundaries; driver-internal transfers and VRAM use remain unmeasured. No zero-copy claim is
supported by this data.

Independent FFmpeg decode succeeded. `ffprobe -count_frames` found exactly 60 and 240 video
frames at the requested resolutions and rates. Paired video PSNR was 40.97 dB mean for the
one-second screen recording and 25.76 dB for noisy 4K `testsrc2`; the latter is a lossy
re-encode of a deliberately grainy source and is not a visual acceptance threshold. The
additional 1080p tone fixture exported 479/479 video frames, and FFmpeg `apsnr` measured
170.136 dB for both decoded audio channels over 20 seconds.

The benchmark script retains per-run exports and logs and accepts a repeat count. This
single run does not establish stable percentiles or regression limits. Those require
repeats under comparable load before changing the export pipeline.
