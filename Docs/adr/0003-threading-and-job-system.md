# ADR-0003 — Threading model and job system

- **Status:** Accepted (2026-10-02)
- **Milestone:** M0 (job system); M2–M4 (decode, playback and audio threads)

## Context

The UI thread must never block on decode, encode, thumbnails, waveforms, probing, cache,
proxies or imports (CLAUDE.md §13). Threads created ad hoc inside components make
cancellation, progress and shutdown unpredictable. Research showed that skimming and the
storyboard zoom depend on cancelling thumbnail work quickly (`Docs/Research/imovie.md` §6).

## Decision

### Process threads
| Thread / pool | Responsibility | Rules |
|---|---|---|
| UI (main) | QML, events, command dispatch | Never blocks; receives results through a single queue mechanism (M6) |
| Render (Qt) | Scene graph, preview presentation | Synchronizes with the compositor through semaphores (ADR-0005) |
| Playback | Clock, frame scheduling | Bounded queues to decode; cancels on seek (M4) |
| Decode | One pipeline per active stream | Bounded queue (backpressure) (M2) |
| Audio RT | PipeWire callback | No allocation, no contended lock, no logging (M4) |
| **JobPool** | Probe, thumbnails, waveforms, proxies, imports, export | Implemented in M0 |

Decode, playback and audio are real-time pipelines with dedicated threads; the JobPool is
for background work without a frame deadline.

### JobPool (`libs/base/jobs.hpp`, implemented in M0)
- `JobPool(n)` creates `n` workers (`std::jthread`). No component creates its own threads
  for background work.
- `submit(name, fn)` returns a `JobHandle`. The function receives `JobContext&` and returns
  `Result<void>`.
- **Cooperative cancellation**: `CancellationSource`/`CancellationToken` (a shared atomic
  flag). The job checks `is_cancelled()` at safe points and returns `ErrorCode::Cancelled`.
  A cancelled pending job never starts.
- **Progress**: `report_progress(fraction)`, read lock-free through the `JobHandle`.
- **States**: `Pending → Running → Succeeded | Failed | Cancelled`. `wait()` blocks and
  returns the job's error.
- **Exceptions**: jobs must not throw; an escaping exception becomes `ErrorCode::Internal`
  and the job ends as `Failed` (no exception crosses the pool boundary).
- **Ordered shutdown** (`shutdown()` and the destructor): stops accepting jobs, cancels
  pending jobs without running them, requests cancellation of running jobs and joins the
  workers. `submit()` after shutdown returns a handle that is already `Cancelled`. It must
  not be called from inside a job.
- Lock order: pool mutex before job mutex; a worker never holds both while running the job
  function.

### Logging across threads
The log sink runs under the logger's internal lock: it must not log, replace the sink or
hold locks that another thread keeps while logging (an inversion detected by TSan during M0).

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
