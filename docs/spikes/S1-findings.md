## Findings

1. **VA-API is the default hardware decode path on Intel TigerLake.** The table reports H.264, HEVC Main 10 and AV1 at the tested sizes; other advertised profiles/codecs need execution checks. The local Omarchy installer selects its driver.
2. **Vulkan Video on ANV/TigerLake is behind a debug flag.** OmaMovie must not depend on users
   (or the app) setting `ANV_DEBUG`, which is a developer switch, not a supported configuration.
   Vulkan Video stays a candidate for newer Intel generations and for AMD (RADV), to be checked
   per device at runtime.
3. **FFmpeg falls back to software silently.** With `-hwaccel vulkan` on a device without video
   queues, decoding still "succeeds" in software. The decode policy must check the format FFmpeg
   actually negotiated (`get_format`) and log the downgrade, never infer success from "no error".
4. **AV1 through Vulkan Video does not initialize even with the flag** (FFmpeg 9 + Mesa 26.2.2,
   8-bit Main, 320×180 and 1920×1080). VA-API and QSV decode AV1 fine. Root cause unknown; not
   blocking because VA-API covers AV1.
5. **Vulkan compute codecs work without any flag**: ProRes decode and FFv1 encode/decode
   stay on the GPU (ProRes encode output is invalid at 1080p, see [S3](S3-decode-paths.md)). This makes a GPU-resident proxy/intermediate path viable on this machine
   (`CLAUDE.md` §15), independent of video decode blocks.
6. **Every interop extension needed for VA-API → Vulkan (DMA-BUF) and timeline semaphores is
   available by default.** Nothing blocks S2.
7. **No hardware AV1 encode** on this GPU; AV1 export here means software (SVT-AV1).

## Draft decode/encode policy for ADR-0004 (Intel TigerLake)

| Content | First choice | Fallback |
|---|---|---|
| H.264, HEVC 8/10-bit, AV1, VP9 decode | VA-API, frames mapped to Vulkan by FFmpeg | Software (logged) |
| ProRes, FFv1 decode | Vulkan compute | Software (logged) |
| H.264/HEVC export | VA-API (or QSV, to be measured in S3) | Software (libx264/libx265) |
| AV1 export | Software (SVT-AV1) | — |
| Proxies | FFv1 or ProRes through Vulkan compute (format chosen in S3) | Software |

Vulkan Video is used only where the device exposes the video extensions **by default** and a
benchmark shows it is not worse than VA-API.

## Open items

- **S2**: does FFmpeg's VA-API → Vulkan mapping work on a `VkDevice` created by OmaMovie, without
  readback? (Interop extensions are present; the device must be created with them.)
- **S3**: measure VA-API vs. QSV vs. (flagged) Vulkan Video decode frame time, CPU and power;
  choose FFv1 vs. ProRes for proxies; measure VA-API vs. QSV encode.
- Investigate the AV1 Vulkan Video failure (FFmpeg vs. Mesa) and report upstream if confirmed.
- Repeat S1 on AMD (RADV, Vulkan Video on by default) and NVIDIA (NVDEC/NVENC) when hardware is available (S7, S8).

[Return to S1-hardware-inventory.md](S1-hardware-inventory.md)
