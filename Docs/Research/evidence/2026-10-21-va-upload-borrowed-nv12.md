# VA-API upload from the borrowed NV12 — 2026-10-21

**Classification:** PERFORMANCE + CORRECTNESS. Same host, build and fixtures; one variable.

Host: i5-1135G7 / Iris Xe (i915), Mesa iHD 26.3.5, FFmpeg 9.0.2, `intel_pstate` `powersave`,
EPP `balance_performance`, `balanced` profile, AC, load under 1.2. Release build,
`build/release/tools/export_bench INPUT OUT` (default route: VA-API H.264, NV12 readback).

**A** = `9d3d2f4`: `VideoWriter::write_nv12` copies the readback rows into its own AVFrame,
then `av_hwframe_transfer_data` uploads that frame. **B** = the same call uploads from an
AVFrame that points at the caller's rows (`Impl::borrowed`); the transfer finishes before
`send_video` returns. libx264's NV12 path still copies: libavcodec would copy a
non-refcounted frame anyway.

Runs alternate A/B, reversing the order every other round. Medians with [min–max].

| Fixture | Runs | Wall A | Wall B | CPU/frame A → B | Encode stage A → B | Row copy A → B |
|---|---|---|---|---|---|---|
| `xed_1080p24_bt709_noise-v3.mp4` (`b555007f…`, 479 frames) | 8+8 | 2468 [2424–2555] ms | 2416 [2164–2463] ms | 5.55 → 4.55 ms | 5.02 → 4.90 ms | 0.5 → 0 ms |
| `dec_h264_2160.mp4` (`0947311c…`, 4K23.976, 72 frames) | 6+6 | 1379 [1311–1399] ms | 1288 [1264–1318] ms | 16.1 → 13.95 ms | 16.18 → 15.07 ms | 1.75 → 0 ms |

The VA-API upload itself did not get slower reading the mapped readback buffer directly
(1080p 2.18 → 1.94 ms, 4K 7.40 → 7.24 ms). Decoded output is identical: per-frame
`framemd5` of A and B match on both fixtures. The 1080p wall-time ranges overlap, so the
claim there is CPU only; at 4K the wall median dropped 6.6% with ranges overlapping by 7 ms.
`export_bench` now prints the per-frame readback, host copy, writer row copy and upload.
This removes one host copy of the staged route; it is not the GPU-native input of
[M12](../../implementation-plan-m12-gpu-export-architecture.md) (`gpu-input`, separate A/B).
