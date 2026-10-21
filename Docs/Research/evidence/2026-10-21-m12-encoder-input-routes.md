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

## Consequence

Route B is the one to pilot: the compositor's NV12 conversion could write planes of encoder-owned
VA surfaces directly, removing the readback, host copies and upload the
[transfer phases](2026-10-16-export-transfer-phases.md) measured. Not shown yet: encoding such a
surface (the surface must reach `h264_vaapi` as an `AV_PIX_FMT_VAAPI` frame of a frames context
that owns it), a pool and reuse synchronisation, writes from the real compute pass, device loss
and cancellation, output correctness against the independent oracle, and any speed figure.
