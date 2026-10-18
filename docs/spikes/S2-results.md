## Results

| Media | Mode | Negotiated format | DMA-BUF | `AVVkFrame` on our device | Frames as `AVVkFrame` / consumed | Luma vs. software | CPU (one core) |
|---|---|---|---|---|---|---|---|
| H.264 | VA-API | `vaapi` | 1 object, 2 layers (R8 + chroma), modifier `0x100000000000002` (Intel Y-tiled) | nv12, 2 images, DRM-modifier tiling, DeviceLocal | 600/600, 600/600 | 64/64 exact | 16% |
| HEVC 10 | VA-API | `vaapi` | same layout, R16 | p010le, 2 images, DRM-modifier tiling, DeviceLocal | 600/600, 600/600 | 32/32 samples exact | 28% |
| AV1 | VA-API | `vaapi` | same layout, R8 | nv12, 2 images, DRM-modifier tiling, DeviceLocal | 600/600, 600/600 | 64/64 exact | 20% |
| H.264 | Vulkan Video (`ANV_DEBUG`) | `vulkan` | — | nv12, 1 multi-planar image, optimal tiling | 600/600, 600/600 | 64/64 exact | 11% |
| HEVC 10 | Vulkan Video (`ANV_DEBUG`) | `vulkan` | — | p010le, 1 multi-planar image, optimal tiling | 600/600, 600/600 | 32/32 samples exact | 21% |
| AV1 | Vulkan Video (`ANV_DEBUG`) | software fallback | — | — | 0 | — | — |

Throughput was 580–1000 fps for 1080p60 including a fence wait per frame. This is an isolated throughput observation, not evidence that the complete path meets playback deadlines; S3 must measure representative end-to-end workloads.

## Findings

1. **FFmpeg 9 accepts a device created by OmaMovie.** Filling `AVVulkanDeviceContext` and calling
   `av_hwdevice_ctx_init()` works; FFmpeg uses our instance, device, extensions and queues.
2. **VA-API → Vulkan has no explicit host transfer in the inspected application path.** The surface is exported as a DMA-BUF and
   imported into our device (memory `DeviceLocal`, tiling `VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT`).
   No `av_hwframe_transfer_data` call happens; CPU use stays at 16–28% of one core for 1080p60
   decode + map + consume.
3. **The `AVVkFrame` synchronization contract works on our queue**: waiting on `sem_value` and
   signalling `sem_value + 1` from OmaMovie's own submissions, frame after frame.
4. **Sampled first-frame luma matches** a software decode for the compared 8-bit and 10-bit values; full-frame/chroma validation remains pending.
5. **`AV_HWFRAME_MAP_DIRECT` breaks VA-API → Vulkan in FFmpeg 9** (`EINVAL`, also on a device
   FFmpeg creates itself). Mapping with `AV_HWFRAME_MAP_READ` alone imports the DMA-BUF. Use no
   `DIRECT` flag.
6. **VA-API frames arrive as two images (one per plane)**, Vulkan Video frames as **one
   multi-planar image**. The compositor's YUV→RGB stage must accept both layouts (separate R8/RG8
   or R16/RG16 images, or a 2-plane image with `PLANE_0/1` aspects).
7. **P010 stores samples in the high bits**; software `yuv420p10le` stores them in the low bits.
   Any comparison or conversion has to account for that.
8. **AV1 needs the native `av1` decoder** for hardware decoding: FFmpeg picks `libdav1d` (software
   only) by default. `libs/media` must select the hwaccel-capable decoder explicitly.
9. **`VK_KHR_internally_synchronized_queues` is supported by ANV**, and FFmpeg 9 deprecates
   `lock_queue`/`unlock_queue` in its favor. Queues created with
   `VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR` and handed to FFmpeg through
   `queue_flags` let FFmpeg and OmaMovie submit to the same queue without an application-level
   lock. Only single-threaded submission was exercised here.
10. **The GPU has a single graphics/compute queue** (family 0, count 1). Qt, FFmpeg and the
    compositor must share it. Risk for S4: queues created with flags can only be fetched with
    `vkGetDeviceQueue2`, and Qt's `QQuickGraphicsDevice::fromDeviceObjects` takes a family and
    index, presumably fetching with `vkGetDeviceQueue` **(verify in S4)**.
11. **Vulkan-Hpp needs `VULKAN_HPP_USE_STD_EXPECTED`** (and `<expected>` included first) to return
    `std::expected` when exceptions are disabled; otherwise it uses its own `ResultValue` type.
    Non-RAII calls that only return `vk::Result` become `std::expected<void, vk::Result>` and
    are `[[nodiscard]]`.
12. **No Vulkan validation layers are installed** on the development machine, so this run is not
    validated. Install `vulkan-validation-layers` and repeat before trusting the synchronization
    code in `libs/gpu`.

[Consequences and open items](S2-consequences.md)

[Return to S2-ffmpeg-own-device.md](S2-ffmpeg-own-device.md)
