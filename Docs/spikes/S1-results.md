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
| `prores_ks_vulkan` (compute) | ⚠️ writes a file, but at 1080p it does not decode ([S3](S3-decode-paths.md)) |

[Return to S1-hardware-inventory.md](S1-hardware-inventory.md)
