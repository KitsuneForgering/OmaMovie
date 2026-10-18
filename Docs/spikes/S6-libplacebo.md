# S6 — libplacebo on OmaMovie's device and images (Intel Iris Xe, TigerLake)

- **Date:** 2026-10-04
- **Milestone:** M1 (`Docs/implementation-plan.md`)
- **Feeds:** [ADR-0006](../adr/0006-color-space-and-libplacebo.md)
- **Reproduce:** `make BUILD=release build/release/spikes/s6_libplacebo`, then
  `build/release/spikes/s6_libplacebo <1080p-video>` (input: `h264.mp4` from
  [S3](S3-decode-paths.md)); add `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` to validate
- **Raw output:** [2026-10-04-s6-libplacebo.txt](../Research/evidence/2026-10-04-s6-libplacebo.txt)

## Question

Can libplacebo run on OmaMovie's `VkDevice` and read its images (compositor output, VA-API
frames) without copies, and what does it cost and change compared with our own shaders for the
stages it could replace (display encode, YUV→RGB with scaling)?

## Environment

Same machine as S3/S5; libplacebo 7.360.1 (Arch package, already an FFmpeg dependency).

## Method

1. `pl_vulkan_import` with `gpu::Device`'s handles, enabled extensions and features, and the
   device's queue lock as libplacebo's lock hooks.
2. Render one 1080p frame with `VulkanCompositor` (RGBA16F, linear, premultiplied), wrap its
   output with `pl_vulkan_wrap`, encode it to sRGB RGBA8 without dithering, and compare with
   `encode_display()` and with the IEC 61966-2-1 formula applied to `read_output()`.
3. Decode one frame through VA-API → Vulkan, hand its plane images to libplacebo with FFmpeg's
   timeline semaphores (`pl_vulkan_release_ex` waiting on `wait_values`, `pl_vulkan_hold_ex`
   signaling `signal_values`, then `GpuAccess::commit`), render YUV→RGB with a downscale to
   1280×720 at the fast/default/high-quality presets, and time the compositor doing the same
   with its bilinear sampling.

Times are medians of 20 after 3 warm-up runs, wall time to `pl_gpu_finish` / `render()`.

## Results

| Stage | Ours | libplacebo |
|---|---|---|
| Device import | — | OK with zero-flag queues (see finding 2) |
| Display encode 1080p (2 runs) | 0.56–0.58 ms | 0.85–1.80 ms |
| Display encode vs sRGB formula | ≤ 1 level | ≤ 4 levels (16 % of samples differ from ours) |
| YUV→RGB + 1080p→720p | 0.89–0.96 ms (bilinear) | fast 0.60–0.89, default 2.77–3.18, high quality 7.17–7.22 ms |

A run under the Khronos validation layer reported no errors from libplacebo, the compositor or
the semaphore handoff; the only message was FFmpeg's known VA-API mapping leak at
`vkDestroyDevice` (ADR-0004). Earlier, separate runs varied by 2–4× (for example the display
encode measured 3.55 ms for ours and 3.43 ms for libplacebo once), so these are orders of
magnitude, not a ranking.

## Findings

[Continue in S6-findings.md](S6-findings.md)
