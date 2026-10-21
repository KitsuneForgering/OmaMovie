# ADR-0023 — GPU-native export input through encoder-owned VA-API surfaces

**Status:** Proposed 2026-10-21 (spikes and a compositor API on `feature/first-release`; the
export path is unchanged; awaits maintainer acceptance). Settles M12 step 3's route choice for
the Intel reference host.

## Context

Export with VA-API encoding composites on the GPU, reads the NV12 back into host memory
(`read_nv12`), copies its rows and lets FFmpeg upload them into an encoder surface
([transfer phases](../Research/evidence/2026-10-16-export-transfer-phases.md)). M12 step 3 asked
to compare FFmpeg 9's own Vulkan → DRM PRIME → VA-API mapping with a direct DMA-BUF import, and
to record the working route before replacing the writer path.

## Decision

- The hardware encoder owns its surfaces: FFmpeg's `h264_vaapi` keeps its VA-API frame pool.
  Each pool surface is exported once as DMA-BUF (`vaExportSurfaceHandle`, read-write, one layer
  per plane) and its planes are imported into OmaMovie's Vulkan device as writable images with
  the surface's own DRM modifier, the reverse of the viewer's existing read-only import.
- `VulkanCompositor::write_nv12` writes the last render's NV12 (the same encoding as
  `read_nv12`) into those planes, acquiring them from and releasing them to
  `VK_QUEUE_FAMILY_FOREIGN_EXT` around the write. The same `AVFrame` then goes to the encoder.
- FFmpeg's Vulkan → VA-API mapping is not used: on this host it fails in every form tried.
- `read_nv12` and the CPU-frame writer stay as the fallback for software encoding, for devices
  or drivers where the import or a writable modifier is unavailable, and after device loss.
- Validated on Intel only (Mesa 26.2.4, iHD 26.3.5, FFmpeg n9.0.2). Other vendors need their own
  evidence; nothing here implies AMD or NVIDIA support.

## Evidence

[Route evidence](../Research/evidence/2026-10-21-m12-encoder-input-routes.md):
- FFmpeg's mapping: iHD refuses its exported DMA-BUF, VA-API cannot be derived from Vulkan, and
  reverse mapping crashes FFmpeg.
- `s7_va_encode_surface`: an encode surface's Y-tiled planes accept R8/RG8 storage writes, and
  GPU-written values read back exactly through VA-API.
- `s8_va_encode_pool`: 120 GPU-written pool surfaces encode and decode within one level.
- `s9_compositor_to_encoder`: the compositor's NV12 written this way and the same frames sent
  through today's readback route give byte-identical movies at 320×180, 1080p and 4K.
  Handing a 4K frame to the encoder took 2.1–2.5 ms instead of 9.2–10.5 ms. That is one stage,
  not an export speed-up figure.
- `tests/compositor/test_nv12_planes.cpp`: `write_nv12` matches `read_nv12` byte for byte.

## Consequences and open work

Before the export uses this route:
- **Ownership:** surfaces, imports and views are owned for the encoder pool's lifetime and
  released before the device and the VA display.
- **Synchronisation:** the per-frame CPU wait (fence, then `vaSyncSurface`) gives way to
  synchronisation that lets compositing and encoding overlap, without writing a surface the
  encoder still reads.
- **Failure paths:** device loss falls back to `read_nv12` and the CPU writer mid-export;
  cancellation and a failed import or write downgrade with an observable reason in
  `ExportStats`.
- **Checks:** the existing export tests and the independent decoded-frame oracle must pass on
  the new route, and the export transfer baseline is measured again on the same fixtures.

Reverting means keeping the readback route, which stays in place throughout.
