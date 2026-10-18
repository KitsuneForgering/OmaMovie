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

[Findings and limits](S3-findings.md)

[Return to S3-decode-paths.md](S3-decode-paths.md)
