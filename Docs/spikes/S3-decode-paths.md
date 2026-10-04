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

### Decode throughput (median of 5)

| Media | Software | VA-API | QSV | Vulkan Video (`ANV_DEBUG`) |
|---|---|---|---|---|
| H.264 8-bit | 792 fps, 568 % CPU | 715 fps, 24 % | 664 fps, 17 % | 729 fps, 23 % |
| HEVC Main 10 | 676 fps, 615 % | 1255 fps, 52 % | 1147 fps, 28 % | 1258 fps, 61 % |
| AV1 8-bit | 566 fps, 241 % | 1085 fps, 28 % | 998 fps, 27 % | fallback to software |

### Proxy candidates (from the H.264 file)

| Proxy | Bitrate | PSNR | Software decode | Vulkan compute decode | Open + frame / seek middle + frame |
|---|---|---|---|---|---|
| ProRes LT 4:2:2 10-bit | 96.9 Mbit/s | 46.5 dB | 617 fps, 612 % | 240 fps, 27 % | 74 / 86 ms |
| FFv1 4:2:0 level 4 | 83.5 Mbit/s | lossless | 97 fps, 638 % | **19 fps**, 3 % | 224 / 554 ms |

Long-GOP sources for comparison (software): H.264 95 / 162 ms, HEVC 112 / 298 ms, AV1 69 / 211 ms.

### `prores_ks_vulkan` output does not decode at 1080p

FFmpeg 9.0.1's Vulkan ProRes encoder writes 1080p files (from `testsrc2` or from decoded
H.264) that neither the software nor the Vulkan decoder accepts: "slice out of bounds, error
decoding picture header", for profiles proxy, LT and HQ. A 320×240 encode decodes. S1 counted
this encoder as working because it checked that the file existed and that `ffprobe` reported
a stream, not that it decoded; that row is corrected by this spike. The script rechecks it on
every run.

## Findings

1. **QSV adds nothing VA-API does not already give.** Both drive the same fixed-function
   block; QSV was 7–12 % slower in throughput and somewhat lighter on CPU. Keep the
   ADR-0004 order (VA-API → Vulkan Video → software) and do not add a QSV path or the libvpl
   dependency for decode.
2. **Hardware decode is about CPU, not throughput, for 1080p60 on this machine.** Every path
   is several times above 60 fps here. H.264 software decode is even faster than VA-API on
   this synthetic content, but takes 5–6 cores; the hardware paths take a quarter to half of
   one. `testsrc2` is easy to decode, so software numbers are an upper bound: real camera or
   screen-recording content will cost more CPU. This supports the M4 hardware decode pilot as
   a CPU/power measure, and still requires its end-to-end gate.
3. **AV1 through Vulkan Video still falls back** with `ANV_DEBUG` (S1 finding 4, reproduced).
4. **ProRes, not FFv1, is the proxy candidate.** ProRes is intra-only, so a seek costs about
   one frame (86 ms vs 74 ms at the start, mostly process startup); FFv1 seeks 2.5× slower
   than opening and its Vulkan decode runs at 19 fps, below real time. FFv1 stays useful as a
   lossless intermediate, not as an editing proxy.
5. **ProRes proxies must be encoded on the CPU for now**, because the Vulkan encoder output
   is invalid at 1080p. Vulkan compute decode of ProRes costs 27 % of one thread at 240 fps
   (software: 617 fps at six threads), so the GPU remains the cheaper way to play them.
6. **Power was not measured.** RAPL (`/sys/class/powercap/intel-rapl:0/energy_uj`) is
   root-only on this system; the script reports joules when it is readable. Power claims for
   any path stay open.

## Limits

Throughput is decoder-only: no mapping into OmaMovie's device, no compositing, no
presentation, no concurrent streams. One synthetic source per codec, one machine, frequency
scaling and thermals uncontrolled. Encode paths for export (VA-API vs QSV vs software) were not
measured; they belong to M7.
