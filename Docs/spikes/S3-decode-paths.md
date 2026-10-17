# S3 — Decode paths and proxy intermediates (Intel Iris Xe, TigerLake)

- **Date:** 2026-10-04
- **Milestone:** M1 (`Docs/implementation-plan.md`)
- **Feeds:** ADR-0004 (decode policy), ADR-0009 (cache/proxies), M8 proxies
- **Reproduce:** `tools/spikes/s3-decode-paths.sh [output-dir]` (`RUNS`, `SECS`)
- **Raw output:** [2026-10-04-s3-decode-paths.txt](../Research/evidence/2026-10-04-s3-decode-paths.txt)

## Question

Is QSV worth a decode path next to VA-API on this machine, how much CPU does each
hardware path save over software decode, and which compute-decodable intermediate (FFv1 or
ProRes) is the better proxy candidate?

## Environment

Intel Core i5-1135G7 (4 cores, 8 threads), Iris Xe (TGL GT2), Mesa 26.2.2 (ANV), iHD 26.2.4,
FFmpeg n9.0.1, kernel 7.2.5-4-omarchy. Same stack as [S1](S1-hardware-inventory.md).

## Method

- Media: 10 s of 1080p60 `testsrc2` (600 frames), GOP 120: H.264 8-bit (libx264 veryfast),
  HEVC Main 10 (libx265 ultrafast), AV1 8-bit (SVT-AV1 preset 12).
- Decode with `-hwaccel X -hwaccel_output_format X -f null`: hardware frames stay on the GPU
  and are discarded. A path is measured only when FFmpeg negotiated its hardware format
  (S1 method). Median of 5 runs of FFmpeg's `-benchmark` rtime and utime+stime; CPU % is of
  one thread.
- Proxies made from the H.264 file: ProRes LT 4:2:2 10-bit (`prores_ks`, CPU) and FFv1
  4:2:0 8-bit level 4 (`ffv1_vulkan`). Quality as PSNR against the source frames paired by
  index; seek as wall time from process start to one decoded frame, at the start and after
  `-ss 5.5`.

## Results

[Measurements](S3-results.md)
