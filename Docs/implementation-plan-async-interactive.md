### Async interactive work (within M11, opened 2026-10-14)

Rule: async in execution, not in semantics. Time, `TimeMap`, timeline commands, validation,
composition evaluation, `RenderGraph` and migrations stay synchronous and deterministic; only
the execution of work they describe moves, and its result is published on the UI thread.

**Problem (measured):** paused hardware scrub freezes the UI ([evidence status](implementation-plan-evidence-2026-10-14.md)).
**Location:** `Session::requestFrame` → `PreviewItem::setRequest` → `composite()` runs
`build_viewer_frame` in `updatePaintNode`, Qt's sync phase, while the GUI thread is blocked.
Waits inside it: VA-API decode-forward from the keyframe (decoder + CPU parsing, up to a GOP),
DMA-BUF import (object creation, no queue use) and the composite's submission (queue).

**Ownership today**

| Object | Owner / thread |
|---|---|
| `gpu::Device`, its one graphics queue | `main` (outlives `Session`); queue under `Device`'s recursive lock, held by the Qt bridge from `afterRendering` to `afterFrameEnd` (ADR-0005) |
| Playback `FrameSource` + decoders | `VideoScheduler`'s one-thread pipeline pool; decode admitted per frame |
| Paused software `FrameSource` | `Session::workers_` (one thread shared with imports, saves, previews) |
| Paused hardware `FrameSource` | `PreviewItem`, Qt render thread |
| GPU frames | `shared_ptr<VideoFrame>` in `ViewerFrame`; imported surfaces keep their DMA-BUF and semaphores until released |

**Three semantics**

1. Playback: ordered, prefetched, throughput-first; stale frames dropped by `take()`. Unchanged.
2. Interactive seek/scrub: latest request wins, latency first; a superseded request stops at
   the next decoded frame (`VideoDecoder::set_interrupt`) and never publishes. One request in
   flight and one wanted (`Session::wanted_`), so a burst 41, 42, 43, 44 runs 41 (stopped),
   then 44; 42 and 43 never start.
3. Background (probe, thumbnails, waveforms, relink, autosave I/O): FIFO `JobPool`, cooperative
   cancellation (ADR-0003). Priorities only when a measured stall needs them.

**Cancellation contract:** a request carries the request count it was issued under and the
session `generation_`; checkpoints between decoded frames ask *still current?* (count),
*session alive?* (`JobPool` shutdown cancels), *device alive?* (`Device::lost`). A cancelled
or stale result publishes no frame, edits nothing and adds no history.

**GPU constraint:** no new queue users. VA-API decode and import may run on a worker without
admission because they submit nothing; composition stays on the render thread. Vulkan Video
decode submits to the queue and stays admitted. More queues need an ADR and validation.

- [x] Spike: libuv vs `JobPool` (2026-10-14): equal latency (171.8 vs 171.5 ms), no capability
  gained; not adopted ([ADR-0020](adr/0020-async-execution-boundary.md)).
- [x] Paused hardware frames decoded on the job worker with the software path's contract;
  the render thread only composites. *Done (2026-10-14, [evidence](Research/evidence/2026-10-14-async-scrub.txt))*:
  UI gap p95 221 → 12 ms (1080p), 1323 → 12 ms (4K H.264), 254 → 16–19 ms (4K HEVC); both
  UI gates pass; first-frame latency unchanged at 1080p. Cancellation latency (now in the audit): 4K
  hardware 150 → 5 ms p50 / 8.5 ms p95 after keeping stopped decoders open; gate passes. Remaining: the 4K composite in sync (~11 ms). *Where it goes (2026-10-15)*: `PreviewItem::composite` runs from `updatePaintNode`, in Qt's sync phase while the GUI thread is blocked, and `VulkanCompositor::render` submits and waits; a bare 4K render takes 8.2-8.9 ms from submit to completion on the Iris Xe ([pass cost](Research/evidence/2026-10-14-m10-effect-pass-cost.txt)), so nearly all of it is GPU time, not recording. Unblocking the GUI means compositing in the render phase (`QQuickWindow::beforeRendering`, still Qt's render thread, so the swapchain argument of ADR-0005 holds) with the frame handed over in sync; that amends ADR-0005's threading and waits for a decision, since the UI gates already pass (4K gap p95 12 ms < 33 ms).
- [x] Tests (`tests/app/test_paused_frames.cpp`): order of publication, final frame, project
  switch and destruction with a seek in flight, device loss; software and VA-API. Validation
  and synchronization validation, TSan and ASan/LSan clean.
- [x] Scrub's own worker (2026-10-14): with 15 imports queued on the shared worker, 6–10 of
  24 seeks showed no frame in 500 ms; on a dedicated one-thread `JobPool`, 0.

**Gates (proposed before measuring):** paused scrub UI event-loop gap p95 < 16.7 ms at 1080p
and < 33 ms at 4K; a superseded request stops within one decoded frame and its cancellation
latency is logged; 0 stale frames published; exact and VFR seek tests unchanged; TSan,
ASan/LSan, validation and synchronization validation clean.

[← Implementation plan](implementation-plan.md)
