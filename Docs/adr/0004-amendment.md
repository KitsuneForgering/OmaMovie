## Amendment (2026-10-06): OmaMovie imports VA-API surfaces itself

FFmpeg 9.0.1's VA-API → Vulkan map (`hwcontext_vulkan.c`, `vulkan_map_from_drm_frame_sync`)
keeps the last mapped frame referenced from its frames context's execution context, and that
frame references the context: every closed hardware decoder leaked its last frame, the derived
context with its command pools, and the VA-API frame and surface pool behind it (found with
the validation layer at `vkDestroyDevice`, reproduced in `tests/media`). A public-API
workaround failed. The maintainer chose to import the surfaces in OmaMovie:

- FFmpeg only exports the surface to DRM PRIME (VA-API alone; with read access it waits for
  decoding with `vaSyncSurface`), as separate single-plane layers.
- `gpu::DmaBufImage` imports each layer: an image with the layer's explicit DRM format
  modifier over the duplicated DMA-BUF descriptor, dedicated memory, no copy.
- `media::VideoFrame` keeps the DRM mapping (which keeps the surface and descriptors) and
  destroys the images first, after waiting for its own timeline semaphores (one per image,
  the same `GpuImages` contract consumers already follow).
- The images are another driver's memory: `GpuImages::foreign` tells consumers to acquire them
  from `VK_QUEUE_FAMILY_FOREIGN_EXT` before use and release them after
  (`gpu::acquire_foreign/release_foreign`, `VK_EXT_queue_family_foreign` enabled), in layout
  GENERAL. The compositor and the test consumer do so.

Evidence (Iris Xe): H.264, 10-bit HEVC and AV1 still match software decode luma exactly and the
compositor matches its CPU reference; `tests/media`, `tests/compositor`, `tests/gpu` and a
15 s hardware-decode run of the editor under the validation layer with synchronization
validation report no errors and no objects left at `vkDestroyDevice`. The import happens per
frame (memory import and two semaphores), as FFmpeg's map did; caching imports per pooled
surface is the next step if a profile shows the cost. Vulkan Video frames still come from
FFmpeg's Vulkan frames.

[Return to 0004-gpu-frames-sync-decode-policy.md](0004-gpu-frames-sync-decode-policy.md)
