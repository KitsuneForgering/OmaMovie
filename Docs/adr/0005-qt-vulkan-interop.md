# ADR-0005 — Qt Quick and the application-owned Vulkan device

**Status: Accepted (2026-10-03).** The S4 diagnostic validated device import;
the editor's viewer now presents compositor images this way (see
*Implementation in the viewer*). Admission for producers on other threads
(hardware decode, export) remains implementation work.

## Context

ADR-0004 chose one application-owned `VkDevice` and internally synchronized
queues when available. The installed Qt 6.11.2 Vulkan RHI, when given an
external device, calls `vkGetDeviceQueue`. Vulkan requires zero queue creation
flags for that retrieval call; flagged queues need `vkGetDeviceQueue2`. Qt's
native handles expose the graphics queue for inspection, not injection. See
[S4](../spikes/S4-qt-shared-device.md) and the [Q1 evidence](../Research/skeptical-review.md#q1-qt-queue-integration).
The precise interfaces are in [Qt's tagged RHI source](https://github.com/qt/qtbase/blob/v6.11.2/src/gui/rhi/qrhivulkan.cpp),
[Qt's RHI native-handle header](https://github.com/qt/qtbase/blob/v6.11.2/src/gui/rhi/qrhi_platform.h)
and the [Vulkan queue retrieval rule](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceQueue.html).

## Decision

Keep OmaMovie as owner of one device. For a Qt Quick bridge using the installed
Qt API, request zero-flag queues through `gpu::DeviceOptions`. The option leaves
existing headless users of ADR-0004 unchanged. With zero-flag queues, all
concurrent queue access requires external host synchronization. The Qt bridge
locks OmaMovie's graphics queue from `beforeFrameBegin` through `afterFrameEnd`
using direct render-thread signal connections. The installed Qt threaded render
loop places `endFrame` (including presentation) inside that interval. Producers
must stop and drain before Qt creates/resizes/destroys its swapchain, because Qt
calls `vkDeviceWaitIdle` outside the signals. S4 proves this admission sequence
with a separate job submitting 120 command buffers while the window redraws.
M4 must implement the same boundary for actual decoded frames and arbitrary
window lifecycle events; the S4 job is not a playback scheduler.

The compositor owns its linear RGBA16F image. The offscreen diagnostic imports
it through `QRhiTexture::createFrom`; the wrapper dies before the borrowed
Vulkan image or device. Readiness is synchronous in that diagnostic, and in
the viewer below. The sRGB display encode does not establish color accuracy;
ADR-0006 must define the SDR/HDR policy.

## Implementation in the viewer (2026-10-03)

The viewer composites on Qt's render thread, in `updatePaintNode` during sync,
and Qt samples the compositor's display image through `QRhiTexture::createFrom`
(`apps/omamovie/src/preview_item.*`). This meets the admission requirement by
construction for the viewer: the thread that submits the compositor's work is
the one that creates, resizes and destroys the swapchain, so they never overlap
Qt's `vkDeviceWaitIdle`.

- Sync runs inside the `beforeFrameBegin`…`afterFrameEnd` interval in which the
  bridge holds the graphics queue, so `gpu::Device`'s queue lock is now
  recursive: the render thread re-enters it to submit; other threads still wait.
- The compositor submits on the same graphics queue as Qt. Its next display
  encode begins with an `ALL_COMMANDS` barrier, so it waits for Qt's earlier
  sampling; Qt's sampling is submitted after the encode and sees its writes.
- A display-size change waits for the queue to go idle before the old image is
  destroyed; the scene-graph node owns Qt's texture wrapper, and the item
  destroys the compositor on `sceneGraphInvalidated` after the queue is idle.
- The display image is RGBA8 encoded with the sRGB transfer function
  (`encode_display`); ADR-0006 still owns the color policy.
- Validation: the GUI smoke run under the Khronos validation layer with
  synchronization validation reports no errors when no window capture is
  taken. `QQuickWindow::grabWindow` (screenshots only) triggers swapchain
  present/acquire hazards in Qt itself, with or without the viewer compositing.
- Decoding stays in software for now (one plane upload per frame): hardware
  frames come from a decode thread and need the admission protocol above.

## Admission for producers on other threads (2026-10-05)

The hardware decode pilot now decodes on `VideoScheduler`'s own thread, ahead of
the clock, instead of on the render thread. Two rules make that safe:

- **Admission gate** (`gpu::Device::admit/leave/close_admission/open_admission`).
  Qt creates, resizes and destroys the swapchain and calls `vkDeviceWaitIdle`
  between frames, outside `beforeFrameBegin`…`afterFrameEnd`. The bridge closes
  admission before the window exists and in `afterFrameEnd` (after releasing the
  queue, since admitted work may be waiting for it), and opens it in
  `beforeFrameBegin`. Closing waits for admitted work to leave and takes
  precedence over new admissions. The producer is admitted around each frame it
  prepares; its wait is timed and checks the job's cancellation, so stopping
  playback never hangs when Qt stops rendering. Admission is open by default:
  headless users (tests, spikes, export) are unaffected.
- **Narrower queue hold.** Qt's own queue operations in a frame are `endFrame`'s
  submit and present, so the bridge now holds the graphics queue from
  `afterRendering` to `afterFrameEnd` instead of for the whole frame. On the
  Iris Xe there is one queue family with one queue, so FFmpeg's VA-API→Vulkan
  mapping submits on Qt's queue; holding it from `beforeFrameBegin` (including
  Qt's wait for the next image) starved the producer: a 20 s 1080p30 audit fell
  5 s behind the audio, and 13 s behind with admission always open, which
  located the cause in the queue hold rather than the gate. The compositor still
  submits during sync under its own (recursive) lock.

- **A flag, not a count.** Qt does not always pair `afterRendering` with
  `afterFrameEnd`: the GUI smoke's window capture renders outside the normal
  frame. Counting the recursive queue lock left one level held, so the next
  admitted decoder waited for the queue while the render thread waited for the
  decoder to leave (found 2026-10-06 as a hang of the hardware GUI smoke,
  located with thread backtraces). The bridge now takes the queue at most once
  per frame and releases it at the frame end or, at the latest, when the next
  frame begins.

Evidence (Iris Xe, release, 1080p30 H.264, PipeWire): with both rules, a 20 s
`--m4-audit` with `OMA_PREVIEW_HARDWARE=1` passed (597 composites, p99 error one
frame, no underruns, one dropped frame); a 15 s run under the Khronos validation
layer with synchronization validation reported no threading or synchronization
error. It did report objects left alive at `vkDestroyDevice` (one mapped NV12
frame and an FFmpeg execution pool); `tests/media` shows the same with VA-API
decoding alone, so that is an existing `libs/media` lifetime defect, tracked in
the plan. Not covered: device loss, export on another thread, 4K/HEVC/AV1 and
long or seek-heavy hardware runs, and whether Qt ever touches the queue before
`afterRendering` in paths not exercised here (window capture calls
`QRhi::finish`, which already conflicts with presentation, see above).

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
