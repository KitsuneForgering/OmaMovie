# ADR-0004 — GPU frame abstraction, synchronization and decode policy

- **Status:** Accepted (2026-10-03)
- **Milestone:** M2 (`libs/gpu`, `libs/media`); revisited when NVIDIA/AMD hardware is measured

## Context

The pipeline is GPU-first (CLAUDE.md §7): decoded frames should reach the compositor as Vulkan
images on OmaMovie's device, without a CPU copy. Spikes S1 and S2 (`Docs/spikes/`) measured the
reference machine (Intel Iris Xe, Mesa 26.2, FFmpeg 9.0.1):

- VA-API decodes H.264, HEVC 8/10-bit, AV1 and VP9 by default; Vulkan Video is only exposed
  behind `ANV_DEBUG=video-decode`, and AV1 fails there even then.
- FFmpeg accepts a `VkDevice` created by OmaMovie and imports VA-API surfaces into it through
  DMA-BUF without a copy (`av_hwframe_map`, without `AV_HWFRAME_MAP_DIRECT`).
- FFmpeg silently continues in software when the hardware refuses a stream (profile, size),
  so the negotiated format has to be checked.
- `VK_KHR_internally_synchronized_queues` lets FFmpeg, the compositor and Qt submit to the same
  queue without an application lock; FFmpeg 9 deprecates its queue-lock hooks in its favor.

## Decision

### One device, owned by `libs/gpu`
`gpu::Device` creates the only `VkInstance`/`VkDevice`. It enables what the driver offers among
the interop (DMA-BUF, DRM modifiers, external memory/semaphore fd) and video extensions, every
queue of every family, and the feature chain FFmpeg reads. Queues are created internally
synchronized when supported; otherwise `Device::lock_queue`/`unlock_queue` provide one mutex per
queue, which FFmpeg receives through its (deprecated) hooks and every other submitter uses.

### Frames: `media::VideoFrame` over `AVVkFrame`
A move-only `VideoFrame` keeps FFmpeg's frame alive and exposes OmaMovie types only: timestamps
as `RationalTime`, the sample layout, and either CPU planes (software) or Vulkan images (GPU).
GPU access goes through `VideoFrame::acquire_gpu()`, which returns a `GpuAccess` holding FFmpeg's
frame lock. It follows FFmpeg's contract for every image: wait on `semaphores[i]` at
`wait_values[i]`, signal `signal_values[i]`, then `commit(layout, access)`. Without a commit the
frame state is left untouched, so an abandoned access cannot leave a semaphore value nobody will
signal. Frames come as one multi-planar image (Vulkan Video) or one image per plane (VA-API).

### Decode policy
`VideoDecoder` tries, in order: **VA-API imported into Vulkan**, **Vulkan Video**, **software**.
A path is skipped at open when its prerequisites are missing (no device, no DMA-BUF import, no
hwaccel for the codec, no video queue for the codec). The driver confirms a hardware path only
on the first frame; if it refuses (in `get_format`) or the Vulkan import fails, the decoder
reopens in software from the same position and logs the downgrade. Callers that must not fall
back leave `Software` out and get `Unsupported` instead. AV1 uses FFmpeg's native `av1` decoder
on hardware paths, because libdav1d has no hwaccel.

### Time and seeking
Timestamps become `std::optional<RationalTime>` in the stream timebase (`AV_NOPTS_VALUE` is
`std::nullopt`). `seek(t)` delivers the frame on screen at `t` — the last with `pts <= t` — by
decoding from the previous keyframe with a one-frame lookahead, exact on VFR media.

## Alternatives considered

- **Let FFmpeg create the device.** Rejected: the compositor and Qt would need a second device
  and a copy, or import every frame (S2 showed our device works).
- **Download to CPU when the Vulkan import fails.** Rejected as the default: it hides a slow
  path. Reopening in software keeps the software decoder as the single correctness reference.
- **Vulkan Video first.** Rejected for now: on the reference machine it needs a debug flag and
  fails on AV1. The order is a parameter; it changes per driver once more hardware is measured.

## Consequences

- Consumers (compositor, thumbnails) never see FFmpeg types and must respect the access
  contract; the tests in `tests/media/test_gpu_decode.cpp` consume frames exactly that way and
  compare the luma with a software decode.
- Each decoder wraps the device for FFmpeg on its own; sharing one wrapper per device is an
  optimization for M4 if profiling shows it matters.
- FFmpeg 9.0.1 leaks a reference cycle (~200 KB, once per decoder) when mapping VA-API frames
  into Vulkan, reproducible with plain FFmpeg. The ASan suppressions in `tests/support/lsan.supp`
  name it; remove them once fixed upstream.
- Hybrid laptops: the VA-API device is the default render node for now; it must match the
  selected Vulkan device (`VK_EXT_physical_device_drm`) before v0.1.
