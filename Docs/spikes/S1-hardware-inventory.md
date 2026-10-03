# S1 — Hardware inventory (Intel Iris Xe, TigerLake)

- **Date:** 2026-10-03
- **Milestone:** M1 (`Docs/implementation-plan.md`)
- **Feeds:** ADR-0004 (GPU frame, synchronization, decode policy)
- **Reproduce:** `tools/spikes/s1-inventory.sh [output-dir]`

## Evidence review (2026-10-03)

The results below are **previously reported local experiments**, not rerun by the skeptical
review. Preserve them as historical observations of the named hardware/build/inputs.
The scripts/source exist, but the document does not retain the raw outputs, source-build
provenance and repetitions needed for independent reproduction. Package labels were
re-observed in [the environment snapshot](../Research/evidence/2026-10-03-local.txt);
they do not resolve the upstream-release discrepancy recorded in the audit.

Capability listings and actual successful decodes are separate evidence. VP9 is advertised
in the text but absent from the executed-decode table; do not count it as tested playback.
Flags/profile lists, resolutions and codec claims are limited to this setup. Extension
presence is necessary but insufficient for mapping. Compute-codec success does not select
a default proxy format without seek/quality/storage measurements. AMD defaults and NVIDIA
behavior remain unmeasured here.

## Question

Which codecs and profiles does the development machine decode and encode, through which
API (VA-API, QSV, Vulkan Video, Vulkan compute codecs), and with which caveats?

## Environment

| Component | Version |
|---|---|
| GPU | Intel Iris Xe Graphics (TGL GT2), device `0x9a49`, integrated |
| Kernel | 7.2.5-3-omarchy |
| Vulkan driver | Mesa 26.2.2 (ANV), Vulkan 1.4.354 |
| VA-API driver | intel-media-driver (iHD) 26.2.4, libva 2.24 |
| QSV | libvpl 2.17.0, vpl-gpu-rt 26.2.4 |
| FFmpeg | n9.0.1 (Arch build: `--enable-vulkan --enable-libvpl --enable-vaapi --enable-libplacebo`) |

## Method

- `vulkaninfo` (extensions, queues, `--show-video-props`) and `vainfo` for advertised capabilities.
- FFmpeg decode of 1 s 1080p test media with `-hwaccel X -hwaccel_output_format X`, reading the
  **pixel format FFmpeg finally chose** (`Format ... chosen by get_format()`, debug log). A
  hardware format means the hardware path ran; a software format means FFmpeg fell back.
- FFmpeg encode of 1 s 1080p test media per encoder, checked with `ffprobe`.
- Vulkan Video probed with and without `ANV_DEBUG=video-decode,video-encode`.

## Results

### Vulkan Video is off by default on this GPU

| ANV configuration | Video extensions | Video queue |
|---|---|---|
| Default | **none** | none |
| `ANV_DEBUG=video-decode,video-encode` | decode H.264, H.265, AV1, VP9; encode H.264, H.265; `video_maintenance1/2` | decode + encode queue |

With the flag, `vulkaninfo --show-video-props` lists (max coded extent 4096×4096 for all):
- **Decode**: H.264 Baseline/Main/High/High 10/High 4:2:2/High 4:4:4 (progressive and interlaced);
  H.265 Main, Main 10, Main Still Picture, RExt 8/10-bit; AV1 Main 8/10-bit (no film grain);
  VP9 Profile 0 and Profile 2.
- **Encode**: H.264 Baseline/Main/High/High 10/4:2:2/4:4:4; H.265 Main, Main 10, Main Still Picture, RExt 8/10-bit.

The interop extensions OmaMovie needs are exposed **by default**:
`VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`, `VK_KHR_external_memory_fd`,
`VK_KHR_external_semaphore_fd`, `VK_KHR_timeline_semaphore`, `VK_KHR_synchronization2`,
`VK_EXT_external_memory_host`.

### Decode (pixel format chosen by FFmpeg)

| Media (1080p) | VA-API | QSV | Vulkan Video | Vulkan Video + `ANV_DEBUG` |
|---|---|---|---|---|
| H.264 High 8-bit | **vaapi** ✅ | **qsv** ✅ | yuv420p ❌ (software fallback) | **vulkan** ✅ |
| HEVC Main 10 | **vaapi** ✅ | **qsv** ✅ | yuv420p10le ❌ | **vulkan** ✅ |
| AV1 Main 8-bit | **vaapi** ✅ | **qsv** ✅ | yuv420p ❌ | yuv420p ❌ (hwaccel init fails) |
| ProRes 422 HQ 10-bit | software (no HW block) | software | **vulkan** ✅ (compute) | **vulkan** ✅ (compute) |
| FFv1 4:4:4 | — | — | **vulkan** ✅ (compute) | **vulkan** ✅ (compute) |

`vainfo` also advertises VP8, VP9 profiles 0–3, HEVC 12-bit/4:2:2/4:4:4/SCC, MPEG-2, VC-1 and JPEG decode.

### Encode

| Encoder | Result |
|---|---|
| `h264_vaapi`, `hevc_vaapi` | ✅ |
| `av1_vaapi` | ❌ (no AV1 encode block on TigerLake; expected) |
| `h264_qsv`, `hevc_qsv` | ✅ |
| `h264_vulkan` (default) | ❌ "Device does not support the VK_KHR_video_encode_queue extension" |
| `h264_vulkan`, `hevc_vulkan` (+ `ANV_DEBUG`) | ✅ |
| `ffv1_vulkan` (compute) | ✅ for yuv444p/bgr0; 4:2:0 at 1080p needs `-level 4 -strict experimental` (subsampling with unaligned height) |
| `prores_ks_vulkan` (compute) | ✅ 4:2:2 10-bit |

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
5. **Vulkan compute codecs work without any flag**: ProRes decode/encode and FFv1 encode/decode
   stay on the GPU. This makes a GPU-resident proxy/intermediate path viable on this machine
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
