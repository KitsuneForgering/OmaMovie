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

[Return to 0005-qt-vulkan-interop.md](0005-qt-vulkan-interop.md)
