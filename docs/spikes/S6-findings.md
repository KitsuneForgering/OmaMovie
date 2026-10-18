## Findings

1. **libplacebo works on our device and images without copies.** It imports the device,
   wraps the compositor image and VA-API plane images, and follows FFmpeg's timeline-semaphore
   contract through its release/hold API.
2. **It needs zero-flag queues, like Qt.** With `internally_synchronized_queues`, libplacebo
   called `vkGetDeviceQueue` and crashed in the driver. With zero-flag queues it crashed again,
   because both it and FFmpeg request the internally synchronized flag from `vkGetDeviceQueue2`
   whenever `VK_KHR_internally_synchronized_queues` is enabled. `libs/gpu` now enables that
   extension only when the queues are created with the flag. The same bug broke VA-API mapping
   for any zero-flag device, which is the configuration the editor uses (ADR-0005), so the
   hardware-decode pilot would have crashed on first use.
3. **Two more `libs/gpu`/`libs/media` defects found on the way, both fixed:** a headless device
   enabled `VK_KHR_swapchain` without `VK_KHR_surface` on the instance (validation error), and
   for VA-API frames `GpuImages::formats[0]` was the multi-planar NV12 format of the whole frame
   although each image holds one plane, so the compositor created image views with a format
   the image was not created with (`VUID-VkImageViewCreateInfo-format-06415`; it worked on ANV
   by accident). Per-plane images now report per-plane formats.
4. **For what OmaMovie does today, our shaders are as good or better.** The display encode is
   closer to the formula (1 vs 4 levels) and not slower; the compositor's bilinear downscale is
   in the same range as libplacebo's fast preset. libplacebo's value is in what we lack:
   higher-quality scalers (its default and high-quality presets cost 3–8× more), dithering,
   tone and gamut mapping for HDR.
5. **libplacebo's C99 parameter macros do not compile as C++**; a C++ integration fills the
   structs field by field.

## Limits

One frame, one machine, no HDR input (none of the stages above involve PQ/HLG), no image
quality metric for scalers beyond timing, no comparison of libplacebo's dithering. The cause of
the 4-level deviation in libplacebo's encode was not investigated.

[Return to S6-libplacebo.md](S6-libplacebo.md)
