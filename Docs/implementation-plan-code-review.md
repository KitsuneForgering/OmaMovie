### Item-by-item code review (2026-10-14)

One module per pass, read through four lenses: algorithmic cost, substance over polish,
engineering risk and the simplest complete fix. Findings cite code; nothing here is
measured unless it says so. Base revision `0e12ece`.

#### 1. `libs/base`

Sound as written: `rescale` is O(1) with 128-bit products, a gcd reduction and checked
overflow (`src/rescale.cpp`); `RationalTime` ordering compares exact fractions; `JobPool`
converts escaping exceptions, cancels pending jobs on shutdown and joins before release;
`DiskCache` verifies the stored key and a checksum, so collisions and torn writes read as
misses, and skips `fsync` correctly for a store that is never a source of truth.

- [x] Keep each disk cache within its budget during a session, not only at startup.
  `DiskCache::put` never trimmed; eviction ran once at launch and on a limit change
  (`apps/omamovie/src/session.cpp:137`, `:502`). *Fixed (2026-10-14)*: `put` counts bytes
  written since the last `evict` and, on crossing a tenth of the budget, runs `evict` on the
  writing (worker) thread; `tests/base/test_disk_cache.cpp` writes 100 KB into a 20 KB store
  and stays under budget plus one tenth. Base tests pass under TSan.
- Not an issue today: `JobHandle::wait` on a cancelled job that is still queued blocks
  until a worker dequeues it. Only playback producers are waited on, in their own pools
  (`video_scheduler.cpp:74`, `audio_player.cpp:137`); revisit if a caller waits on a
  `workers_` job.
- Open for the app pass: §19 asks for rate-limited hot-path logging, but `libs/base` has
  no helper; check whether the app's one-shot `warned_` flags justify one.

#### 2. `libs/gpu`

Sound as written: RAII owners free partial creations; `CommandRunner` refuses work on a lost
device; the admission gate's timed wait is the simplest way to observe a plain cancel flag.
The synchronous submit-and-wait per composite is already scoped by M4 (try bounded frames in
flight only if queue waits dominate), so no new item.

- [ ] Measure the per-frame VA-API import before changing it. Every decoded frame maps the
  surface, then per layer `dup`s the fd and creates an image, an imported allocation and a
  timeline semaphore, all destroyed on release (`libs/media/src/video_decoder.cpp:382`,
  `libs/gpu/src/resources.cpp:328`). The 1080p60 hardware gate passes with 3.5 ms
  composites, so this is not a known bottleneck. Add the import to the audit's stage
  lines; if it matters at 4K60 or several streams, cache imports per VA surface (bounded
  by FFmpeg's pool, dropped when the frames context changes).
- [x] Report GPU memory exhaustion as its own error, not `Unsupported` (`resources.cpp:19`):
  a fallback or notice could not tell a full VRAM from a format the driver refuses. *Fixed
  (2026-10-14)*: `ErrorCode::OutOfMemory` (also FFmpeg's `ENOMEM`), and one
  `gpu::error_code(VkResult)` replaces four copies that disagreed (the compositor reported
  every failure, a lost device included, as `Internal`); `tests/gpu/test_error_code.cpp`.

Continued in [part 2](implementation-plan-code-review-2.md).

[← Implementation plan](implementation-plan.md)
