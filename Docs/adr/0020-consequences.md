## Alternatives

- **libuv** (rejected). A scratch spike ([evidence](../Research/evidence/2026-10-14-async-scrub.txt))
  ran the same burst of 20 scrub requests, 15 ms apart, over a simulated decode-forward of
  3 ms per frame in GOPs of 100: `JobPool` with the session's pattern published the last request
  after 171.5 ms (147.5 frames decoded), `uv_queue_work` with `uv_cancel` after 171.8 ms
  (149.3), a FIFO without cancellation after 2880 ms (1027). `uv_cancel` only cancels work that
  has not started, so a running decode needs the same cooperative flag; libuv adds its own
  process-wide pool (4 threads, sized by an environment variable), a second event loop beside
  Qt's and callback lifetimes, and solves nothing the measured problem needed. Not added as a
  dependency.
- **Admit around the whole paused decode on a worker** (rejected): `close_admission()` at
  Qt's frame end would wait for the seek, and the next sync would block the GUI thread again.
- **Software decode for scrubbing** (rejected): its UI stays free, but it reached fewer
  frames (4K: 11–15 of 24 seeks without a frame in 500 ms) and later ones at 1080p.
- **A dedicated scrub worker** (deferred): the shared job worker also runs imports and saves.
  Not measured to matter yet; measure a scrub during an import first.

## Consequences

- Paused scrub UI event-loop gap p95: 221 → 12 ms at 1080p, 1323 → 12 ms at 4K H.264,
  254 → 16–19 ms at 4K HEVC; first-frame latency unchanged at 1080p (62 / 290 ms p50 / p95).
  Vulkan core and synchronization validation report no errors in the scrub audits or app tests.
- Cancellation latency is measured by the scrub audit. Closing the stopped decoder first cost
  ~150 ms at 4K in hardware; it now stays open and seeks again: 5 ms p50, 8.5 ms p95.
- The remaining UI cost is the composite inside Qt's sync phase (~11 ms at 4K). Moving it out
  of sync is a separate change to ADR-0005's viewer handoff.
- During a continuous drag over long GOPs, a request superseded before it finishes shows
  nothing: the latest frame wins and intermediate ones are not shown. At 4K H.264 with a
  250-frame GOP, 14 of 24 audit seeks needed more than the audit's 500 ms and were superseded.
  A fast nearest-keyframe preview during drags is a candidate, not decided.
- FilmCraft documents the same shape for its monitor (a worker pool, stale jobs dropped, seeks
  skipping non-reference pictures); OmaMovie keeps its own invariants (exact frame, one device
  owned by the app, publication on the UI thread) and copies no mechanism from it. Premiere's
  internals are not public, so no comparison is claimed.
- `PreviewItem::setRequest` and its render-thread decoder are no longer used by the session.

[← ADR-0020](0020-async-execution-boundary.md)
