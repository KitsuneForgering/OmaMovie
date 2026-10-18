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

[Return to S2-ffmpeg-own-device.md](S2-ffmpeg-own-device.md)
