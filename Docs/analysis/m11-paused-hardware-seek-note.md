# M11 note: paused hardware seeks freeze the UI (2026-10-14)

## Measurement

Intel Iris Xe, release build, `baseline_1080p60.mp4` (GOP 250). A temporary variant of
`--m4-seek-audit` (not committed) seeks with the editor paused and records, per seek, the largest
gap between ticks of a 2 ms timer on the UI thread:

| Paused viewer | UI event-loop gap per seek, p50 / p95 / max | first frame p50 / p95 |
|---|---|---|
| hardware (default on Intel) | **120 / 306 / 306 ms** | 120 / 306 ms |
| software | 10 / 51 / 51 ms | 102 / 260 ms |

With hardware decode the UI thread is frozen for the whole seek: nothing repaints and no input
is handled while a seek decodes forward from its keyframe.

## Cause

`Session::requestFrame` hands paused hardware frames to `PreviewItem::setRequest`;
`PreviewItem::composite()` decodes them in `updatePaintNode`, Qt's sync phase, during which the
GUI thread is blocked (ADR-0005's render-thread pilot). A seek there decodes up to a GOP.

## Why moving the decode is not enough

The playback producer already decodes hardware frames on its own thread, admitted per output
frame (`Device::admit`). But `Device::close_admission()`, called by the Qt bridge at the end of
every frame, waits until every admitted producer has left. A paused seek decoded on a worker
under one admission would make the render thread wait for it at frame end, and the next sync
would block the GUI thread again.

## Proposal (needs a decision: it changes ADR-0005's admission protocol)

Admit only around work that touches OmaMovie's `VkDevice` or Qt's queue: importing the delivered
VA-API surface into Vulkan and compositing. VA-API decoding itself, including a seek's catch-up
frames, which are never imported, uses the VA driver, not the Vulkan queue, and could run outside
admission. Then paused hardware frames can be decoded on the playback pipeline thread (as playback
frames are), superseded by newer requests through the decoder interrupt, and handed to the viewer
with `setFrame`, leaving the sync phase only to composite.

To verify before adopting: that VA-API decode and Qt's swapchain recreation (with its
`vkDeviceWaitIdle`) do not interfere when decode runs unadmitted (validation layers, TSan, the
GUI smoke's resize and device-loss steps), and that the gap above drops to the software path's.
Vulkan Video decode (FFmpeg's Vulkan hwaccel) does use the queue and would stay admitted.
