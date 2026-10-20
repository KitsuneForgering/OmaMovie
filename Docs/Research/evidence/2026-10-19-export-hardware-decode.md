# Export hardware decode A/B — 2026-10-19

**Classification:** PERFORMANCE + CORRECTNESS. Same host, build and fixture; one variable.
Follows [the repeats](2026-10-19-export-transfer-repeats.md), which found the staged 4K
export mostly CPU-bound (render engine about 56% busy, about five cores in use).

Host: i5-1135G7 / Iris Xe (i915), Mesa iHD 26.3.5, FFmpeg 9.0.2, `intel_pstate` `powersave`,
EPP `balance_performance`, `balanced` profile, AC. Release build. Fixture `a2160.mp4`
(4K23.976 H.264, 240 frames, sha256 `c6917a4d…`), `--export-audit`, VA-API H.264 encoder,
`tools/bench/cross-editor/measure.py` with DRM fdinfo sampling.

**A** = `19bc873` (export `FrameSource(nullptr)`, software decode). **B** = the export's
`FrameSource` given the device (VA-API → Vulkan import, as the Intel viewer).

| 4K23.976, 240 frames | A: software decode (3 runs) | B: hardware decode (3 runs) |
|---|---:|---:|
| Export duration (ms), min / median / max | 8576 / 9499 / 12860 | 7020 / 7092 / 8188 |
| Process CPU per frame (ms), median | 272 | 43 |
| User CPU (s), median | 65.6 | 12.4 |
| Peak RSS (MB), median | 700 | 487 |
| Render engine busy (s) | 4.94 (one sampled run) | 3.98–4.15 |
| Video engine busy (s) | 1.75 (one sampled run) | 3.86–3.92 |
| Peak GPU-resident memory (MB) | 605 (one sampled run) | 751–847 |

**Correctness.** B's output has 240/240 frames and is pixel-identical to A's (ffmpeg `psnr`
between the two exports: infinite on Y, U and V); against the source both give 25.76 dB mean,
the known figure for this grainy fixture. H.264 decoding is bit-exact by specification, so
identical pictures are expected, and this confirms the import path adds no conversion
difference here.

**Reading.** Hardware decode moves about 6× less CPU per frame and roughly a quarter less
wall time on this fixture; the video engine now carries decode and encode, and GPU-resident
memory rises by 150–250 MB. A and B runs were not interleaved, so treat the
duration change as indicative; the CPU change is far outside the spread.

**Contention probe.** Two B exports run concurrently (separate processes and devices) both
finished correctly (240 frames each, pixel-identical to B) in 12.9 s each, i.e. about 37 fps
aggregate against 34 fps alone: the GPU engines are shared, nothing failed or fell back.
This approximates an export beside the viewer; the GUI smoke (`omamovie --smoke`) now plays
the viewer during its export step and fails if playback does not advance. On this host the
log shows the export decoding H.264 and 10-bit HEVC through VA-API → Vulkan while the viewer
played 0.10 → 0.57 s; raising the required advance to 100 s made the smoke fail as expected.

**Device loss.** With hardware decode, a lost device also invalidates the decoded
pictures. The export now replaces its frame source with a software one at the loss and
decodes that frame again; `tests/app/test_export_device_loss.cpp` checks the switch and that
every frame still matches an all-CPU export.

**HEVC 10-bit and AV1.** `tests/app/test_export_decode_paths.cpp` exports the 320×180
`hevc_10bit.mp4` and `av1_opus.mkv` fixtures through Session with Settings decode
`hardware` and `software` (libx264 both times): per-frame `framemd5` of the outputs is
identical, and the hardware runs decoded no software frames on this host.

**25-minute long export** (`tools/bench/long-export/run.sh 25`, 45,000 1080p30 frames with
reordered cuts, a dissolve and two sepia clips; [raw report](2026-10-19-long-25min-hardware-decode.txt))
against the [2026-10-17 software-decode run](2026-10-17-rc-long-25min.txt) of the same scene:
PASS with the same output size (1,607,220,126 bytes) and the same frame-check values; CPU per
frame 21.45 → 11.03 ms, peak RSS 804 → 358 MB, wall 344.8 → 292.6 s, zero software frames and
staging copies; peak GPU-resident memory 1,050 MB. Different days and starting load (6.8 vs
9.3), so the wall time is indicative; only a peak memory value was sampled, so this run does
not exclude slow GPU-memory growth.

**Memory over time** ([series](2026-10-20-long-25min-memory-series.txt)). A repeat of the same 25-minute export with `MEASURE_SERIES` (one
sample per second; PASS, identical size and checks) shows GPU-resident memory stepping
369 → 447 → 531/650 → 851 MB at about 61, 119–120 and 178 s, i.e. at the edit's first cuts
(each cut is 9,000 frames, about 58 s of wall time; the dissolve sits at the second), then
flat at 847–851 MB through the fourth cut and the last 115 s. Process RSS rose 320 → 356 MB.
The steps match the frame source opening or reseeking decoders (at most two per file,
`kStreamsPerFile`), so the growth is bounded in this run rather than a leak; whether about
850 MB, roughly 280 NV12 1080p surfaces, is a necessary pool or retained imports is an open
question, not a measured defect.

Not covered: AMD/NVIDIA (not validated, so `Auto` stays software there), multi-stream 4K
scenes, two-hour exports, several source files and memory pressure.
