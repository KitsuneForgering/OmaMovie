# S2 — FFmpeg decoding on a VkDevice created by OmaMovie

- **Date:** 2026-10-03
- **Milestone:** M1 (`Docs/implementation-plan.md`)
- **Feeds:** ADR-0004 (GPU frame, synchronization, decode policy), ADR-0005 (Qt Quick ↔ compositor)
- **Code:** `tools/spikes/s2_ffmpeg_own_device.cpp` (`make spikes`)
- **Run:** `build/debug/spikes/s2_ffmpeg_own_device <media> [--mode vaapi|vulkan] [--frames N]`

## Evidence review (2026-10-03)

The results below are **previously reported local experiments**, not rerun by the skeptical
review. Preserve them as historical observations of the named hardware/build/inputs.
The scripts/source exist, but the document does not retain the raw outputs, source-build
provenance and repetitions needed for independent reproduction. Package labels were
re-observed in [the environment snapshot](../Research/evidence/2026-10-03-local.txt);
they do not resolve the upstream-release discrepancy recorded in the audit.

The application path reports no explicit host transfer except a verification readback.
`DeviceLocal` on an integrated GPU does not imply separate VRAM or prove absence of driver
copies. Not using `AV_HWFRAME_MAP_DIRECT` removes a direct-mapping guarantee; inspect source
and trace the actual path before publishing an allocation-level zero-copy claim.
The compared first-frame luma samples do not establish chroma, every pixel/frame, color
management or concurrency. CPU percentage and isolated throughput do not establish the
end-to-end playback budget. Validation layers and multi-submitter stress remain pending.

The Qt queue concern below now has documentary support: see [Q1](../Research/skeptical-review.md#q1-qt-queue-integration).
The leak assertion requires retained unsuppressed output and repeated decoder teardown;
the current broad suppressions are not evidence of a harmless bounded leak.

## Question

Does FFmpeg 9 accept a `VkDevice` created by OmaMovie with Vulkan-Hpp and deliver decoded
frames as `AVVkFrame` on that device, without a CPU readback? How does queue sharing work?

## Setup

Same machine as S1 (Intel Iris Xe TGL GT2, Mesa 26.2.2, iHD 26.2.4, FFmpeg n9.0.1). Test media:
10 s 1080p60 H.264 High, HEVC Main 10 and AV1 Main, generated with `testsrc2`.

What the spike does:
1. Creates `VkInstance`/`VkDevice` with **Vulkan-Hpp** (`vk::raii`, no exceptions,
   `std::expected`): every queue of every family, the extensions FFmpeg can use that the device
   has (`av_vk_get_optional_device_extensions()`), plus `VK_KHR_internally_synchronized_queues`,
   and exactly the supported features (Vulkan 1.1–1.4 feature structs).
2. Hands that device to FFmpeg by filling `AVVulkanDeviceContext` (`inst`, `phys_dev`, `act_dev`,
   `device_features`, `enabled_dev_extensions`, `qf[]`, `queue_flags`) and calling
   `av_hwdevice_ctx_init()`.
3. Decodes with VA-API and maps every frame VA-API → DRM (DMA-BUF) → Vulkan on our device
   (`av_hwframe_ctx_create_derived` + `av_hwframe_map`), or decodes with Vulkan Video directly
   on our device.
4. Consumes every frame **on our own queue**, following the `AVVkFrame` contract: wait on
   `sem[0]` at `sem_value`, transition the image, signal `sem_value + 1`.
5. For the first frame only, copies 64 bytes of luma to a host buffer and compares them with a
   software decode of the same frame (verification, not part of the hot path).

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

## Consequences for the design

- `libs/gpu` owns the device; `libs/media` wraps it for FFmpeg exactly as this spike does. The prototype reports this integration for the tested setup; release/concurrency validation is still pending.
- Decode on Intel TigerLake: VA-API + mapping without `DIRECT` (S1 policy confirmed).
- `Frame` in `libs/media` wraps the mapped `AVVkFrame`, keeps the source VA-API `AVFrame`
  referenced while the image is in use (`CLAUDE.md` §8.1) and exposes the plane layout (separate
  images vs. multi-planar) to the compositor.
- Queue sharing: prefer internally synchronized queues; S4 must confirm Qt can use such a queue,
  or define a fallback (a single application-level lock passed to FFmpeg's deprecated callbacks).

## Open items

- FFmpeg 9.0.1 leaks a reference cycle when mapping VA-API frames into Vulkan (direct mapping
  and through an explicit DRM step alike): the VA-API device, its surface pool and the derived
  Vulkan frames context stay alive, ~200 KB once per decoder. Reproduced with plain FFmpeg and
  an FFmpeg-created Vulkan device under ASan; suppressed in `tests/support/lsan.supp`. Report
  upstream.

- Repeat with validation layers (`vulkan-validation-layers`).
- Multithreaded submission (decode thread + compositor thread) on the internally synchronized queue.
- S4 follow-up: [Qt 6.11.2 requires zero-flag queues](S4-qt-shared-device.md) for its
  imported-device path. The S2 internally synchronized queues remain valid for the
  measured FFmpeg/compositor prototype, but cannot be passed through that Qt API.
- AV1 Vulkan Video failure (also seen in S1).
- A real Omarchy screen recording (`gpu-screen-recorder`) as input.
