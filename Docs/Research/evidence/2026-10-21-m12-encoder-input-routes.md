# M12 step 3: Vulkan output into a VA-API encode surface — 2026-10-21

**Classification:** FEASIBILITY / interop, Intel reference host (i5-1135G7, Iris Xe TGL GT2),
Mesa 26.2.4, intel-media-driver (iHD) 26.3.5, libva 2.24.1, FFmpeg n9.0.2. Bounded finding for
this driver, not a claim for AMD/NVIDIA or other Intel generations.

## Route A: FFmpeg's own Vulkan → VA-API frame mapping

`ffmpeg -init_hw_device vaapi=va:/dev/dri/renderD128 -init_hw_device vulkan=vk@va
-filter_hw_device vk -f lavfi -i testsrc2=s=1920x1080:r=60 -vf
"format=nv12,hwupload,hwmap=derive_device=vaapi" -c:v h264_vaapi`:
- Forward mapping: iHD refuses the DMA-BUF FFmpeg exports from its Vulkan frame
  (`Failed to create surface from DRM object: 2 (resource allocation failed)`), no output.
- Deriving VA-API from an independent Vulkan device (`vulkan=vk:0[,linear_images=1]`):
  `Failed to created derived device context: -38` (not implemented).
- Reverse mapping (`hwmap=derive_device=vaapi:reverse=1`, surfaces allocated by VA-API and
  mapped into Vulkan): FFmpeg segfaults after the first frame.

The route also needs FFmpeg-owned `AVVkFrame`s, not the compositor's own images.

## Route B: VA-API encode surface imported into OmaMovie's Vulkan device

`tools/spikes/s7_va_encode_surface.cpp` (`make spikes`): `vaCreateSurfaces` NV12 1920×1080 with
`VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER`, `vaExportSurfaceHandle` (DRM PRIME 2, read-write,
separate layers), each plane imported into OmaMovie's device as a writable image with the
surface's own modifier, cleared on the GPU between foreign-queue acquire and release, then
`vaSyncSurface` + `vaDeriveImage` read back through VA-API:

```
surface: 1 objects, 2 layers, modifier 0x100000000000002, Y pitch 1920, UV offset 2088960
  R8 with this modifier: transfer-dst yes, storage yes
  RG8 with this modifier: transfer-dst yes, storage yes
read back through VA-API: Y 128, U 64, V 191 (want 128 64 191)
PASS: the GPU wrote the encode surface VA-API reads
```

The surface is I915 Y-tiled (no CCS), one buffer with the chroma plane at 1920 × 1088 (height
aligned to 16); both planes can be compute storage images. This is the reverse of the viewer's
existing VA-API → Vulkan import (read-only there), on the same device and modifier family.

## Route B encoded: the encoder's own pool, written by the GPU

`tools/spikes/s8_va_encode_pool.cpp out.mp4 120`: FFmpeg's `h264_vaapi` gets a VA-API frames
context (NV12 1920×1080, pool of 16); for each frame the spike takes a pool surface, exports and
imports its planes into OmaMovie's device once per surface, clears Y to a gray that changes with
the frame index (16 + 7i mod 220, chroma 128) on the GPU, and sends that same `AVFrame` to the
encoder (constant QP 20, no B-frames, limited range). No host copy and no upload.

```
encoded 120 GPU-written frames from 2 pool surfaces to s8.mp4
stream|codec_name=h264|profile=High|width=1920|height=1080|color_range=tv|nb_read_frames=120
```

An independent FFmpeg decode to raw YUV 4:2:0: all 120 frames, worst sampled deviation 1 level
from the written value in Y, U and V; each frame shows its own index's gray, so a lost,
repeated or reordered frame would fail. The encoder needed only two pool surfaces (it releases a
surface once encoded); the spike waits for the surface with `vaSyncSurface` before writing it.

## Consequence

Route B is the one to pilot: the compositor's NV12 conversion could write planes of encoder-owned
VA surfaces directly, removing the readback, host copies and upload the
[transfer phases](2026-10-16-export-transfer-phases.md) measured. Encoding such surfaces from the
encoder's own pool is shown above. Not shown yet: writes from the real compositor's NV12 pass
(compute storage writes, not a clear), synchronisation without `vkQueueWaitIdle` per frame
(semaphores or a fence per surface), device loss
and cancellation, output correctness against the independent oracle, and any speed figure.
