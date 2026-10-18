## Alternatives considered

- **Pass an internally synchronized queue to the installed Qt.** The import
  API cannot receive that handle and Qt's retrieval call is invalid for a
  flagged queue. A future Qt version or patched RHI needs fresh verification.
- **Separate Qt and OmaMovie devices.** External-memory/semaphore import adds
  another identity, allocation and lifetime contract; there is no measured
  reason to add it while a same-device prototype works.
- **Use a mutex only in OmaMovie's submitters.** Qt does not call
  `Device::lock_queue` itself; the direct frame-signal bridge is necessary.
- **Composite on a worker thread and hand finished images to Qt.** Keeps the
  render thread free but needs the pause/drain protocol for every swapchain
  event and double-buffered images with cross-thread lifetime; deferred until
  a measured render-thread cost justifies it.

## Consequences and acceptance

The shared queue is valid in the repeatable S4 offscreen diagnostic. A
historical static visible run also reported 120 bounded concurrent submissions
and zero validation errors, but its test-pattern UI was replaced. The current
GUI presents compositor images of software-decoded video without readback.
This does not establish zero-copy decode-to-screen playback, 1080p60
new-frame throughput or color accuracy. These remain in S4/M4/S6, along with
hardware-decode concurrency, resize admission and device-loss recovery.

[Return to 0005-qt-vulkan-interop.md](0005-qt-vulkan-interop.md)
