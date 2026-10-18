# S2 — FFmpeg decoding on a VkDevice created by OmaMovie

- **Date:** 2026-10-03
- **Milestone:** M1 (`docs/implementation-plan.md`)
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

[Measurements and findings](S2-results.md)
