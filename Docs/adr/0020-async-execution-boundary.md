# ADR-0020 — Async execution boundary for interactive work

- **Status:** Accepted (2026-10-14) for the paused viewer. Amends ADR-0005's viewer section:
  paused hardware frames no longer decode on Qt's render thread.
- **Milestone:** M11, [async interactive work](../implementation-plan-async-interactive.md)

## Context

Paused hardware scrub froze the UI event loop for 221 ms p95 at 1080p and 1.3 s p95 at 4K
H.264 ([evidence](../Research/evidence/2026-10-14-async-scrub.txt)). `PreviewItem` decoded the
paused frame inside `updatePaintNode`, Qt's sync phase, during which the GUI thread is blocked.
The work in that phase was decoder work (VA-API decoding forward from a keyframe, up to a GOP),
CPU work (parsing, the render graph), object creation (OmaMovie's DMA-BUF import: images,
memory, semaphores; no queue submission) and one queue submission (the composite). Software
decode already ran on `Session`'s job worker with a request counter, an interrupt between
decoded frames and one pending wish, so its UI stayed at 9 ms p50. The cause was where the
hardware work ran, not the decoder: hardware also reached the first frame sooner (63 vs 220 ms
p50 at 1080p).

## Decision

**Async in execution, not in semantics.** Time, `TimeMap`, commands, validation, composition
evaluation, `RenderGraph` and migrations stay synchronous and deterministic. The boundary is:
the UI thread describes the work (immutable timeline snapshot, paths, LUTs, frame), a worker
executes it, and the result is published on the UI thread through `post_to_ui`, where it is
checked against the current state before it changes anything.

Three semantics, three mechanisms already in place:

| Work | Semantics | Mechanism |
|---|---|---|
| Playback | Ordered, prefetched, throughput first; late frames dropped | `VideoScheduler`: its own pipeline thread, bounded queue, decode admitted per frame |
| Seek / scrub (paused viewer) | Latest request wins, latency first | `Session` job worker: one request in flight, one wish pending; the counter stops a superseded seek between decoded frames |
| Background (probe, thumbnails, waveforms, relink, autosave I/O) | FIFO, cooperative cancellation | `JobPool` (ADR-0003) |

**Cancellation contract.** A request records `viewer_requests_` and `generation_` when issued.
Its checkpoints (between decoded frames) ask: still current (counter), session alive (the
job's cancellation, set by `JobPool::shutdown`), device alive (`Device::lost`). Publication
requires the same generation and the same counter. A cancelled, stale or lost-device result
shows no frame, edits nothing, adds no history and does not fail the session; a lost device
blanks the viewer, which reports the loss.

**GPU ownership.** No new queue users. Paused hardware frames are decoded and imported on the
job worker without admission, because VA-API decoding and the import submit nothing to the
graphics queue; Qt's swapchain changes and `vkDeviceWaitIdle` need host synchronization of
queues, not of object creation. Vulkan Video decoding submits, so a device that offers it is
admitted around the decode as in ADR-0005. Composition stays on Qt's render thread in the sync
phase. More queues or submissions from other threads need their own ADR and validation.

[Alternatives and consequences](0020-consequences.md)
