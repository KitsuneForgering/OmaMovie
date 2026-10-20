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
This approximates, but does not reproduce, an export running beside the viewer's own
hardware decode.

**Device loss.** With hardware decode, a lost device also invalidates the decoded
pictures. The export now replaces its frame source with a software one at the loss and
decodes that frame again; `tests/app/test_export_device_loss.cpp` checks the switch and that
every frame still matches an all-CPU export.

Not covered: AMD/NVIDIA (not validated, so `Auto` stays software there), HEVC/AV1 export
sources, multi-stream 4K scenes, long exports and memory pressure.
