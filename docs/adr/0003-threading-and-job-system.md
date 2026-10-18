# ADR-0003 — Threading model and job system

- **Status:** Accepted (2026-10-02)
- **Milestone:** M0 (job system); M2–M4 (decode, playback and audio threads)

## Evidence review (2026-10-03)

The dedicated-pipeline/background-pool distinction remains accepted. Skimming requires
bounded/cancelable work as a design inference, not proven usability. The current JobPool
queue is explicitly unbounded as recorded below; it is not compliant with the future
bounded-background-work target merely because pipeline queues are bounded. Before M6,
measure cancellation latency/stale work and introduce a bound/admission policy if required.

[PipeWire Streams](https://docs.pipewire.org/page_streams.html) specifies that RT-process
callbacks must use RT-safe functions. TSan detects classes of data races/lock issues; it does
not prove callback timing, allocation freedom or audio/video synchronization. M4 must
measure presentation/latency and recovery separately. No thread model is changed here.

## Context

The UI thread must never block on decode, encode, thumbnails, waveforms, probing, cache,
proxies or imports (CLAUDE.md §13). Threads created ad hoc inside components make
cancellation, progress and shutdown unpredictable. Research showed that skimming and the
storyboard zoom depend on cancelling thumbnail work quickly (`docs/Research/imovie.md` (historical research reference; see the dated review note)).

## Decision

[Read the decision](0003-decision.md)

## Alternatives considered
- **`std::async`**: no control over thread count, cancellation or shutdown.
- **QThreadPool**: would couple `libs/base` to Qt (forbidden by CLAUDE.md §5.2).
- **Task libraries (TBB, Taskflow)**: more capable (work stealing, graphs), but a large
  dependency with no current need. Re-evaluate if job graphs appear.

## Consequences
- The JobPool queue is FIFO, unprioritized and unbounded. Priorities (e.g. visible
  thumbnails before off-screen ones) and a queue bound are added when skimming (M6) needs
  them, with measurement.
- Delivering results to the UI thread (a single mechanism, e.g. `QMetaObject::invokeMethod`
  with `Qt::QueuedConnection`) is decided in M6, in the app layer.
- Pool tests run under TSan in CI (`tests/base/test_jobs.cpp`).
