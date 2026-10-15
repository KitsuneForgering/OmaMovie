### Item-by-item code review, part 4 (2026-10-14)

Continues [part 3](implementation-plan-code-review-3.md). Base revision `0e12ece`.

#### Found by the cross-editor bench (2026-10-14)


- [x] **Deadlock: one GPU frame on two layers.** `VulkanCompositor::render` binds inputs by
  layer index, so two layers showing the same `VideoFrame` (one clip stacked over itself, or
  two clips of one file at the same source time) call `acquire_gpu` twice on one thread; the
  frame mutex is not recursive and the render thread waits on itself forever
  (`libs/compositor/src/vulkan_compositor.cpp:809`, `libs/media/src/video_frame.cpp:299`).
  Observed: an `--export-audit` run hung after writing a complete file; the core showed the
  render thread owning the mutex it waited on and a viewer frame of two layers over frame
  `0x7d7a309d17d0`. Fix: reuse the first binding of a frame for later inputs (one acquire,
  one wait/signal, one foreign release); regression test with two layers over one GPU frame.
  *Fixed (2026-10-14)*: a later input showing an already bound frame reuses its views without
  a second access (`BoundInput::alias`); `tests/compositor/test_shared_frame.cpp` renders
  two layers over one VA-API frame twice and matches the CPU reference; without the fix it
  hangs (killed at 60 s). Open: why that one-file audit had two video layers. Headless,
  opening the file and exporting as the audit does leaves one storyline clip and no lane
  clips (`tests/app/test_open_one_file.cpp`), and `evaluate` yields one layer per clip
  outside transitions, so the second layer came from the GUI path. Every viewer path
  (scheduler, paused decoder, scopes, export) builds frames from a timeline with
  `build_viewer_frame`, and none adds a layer by itself; the run's timeline must have held
  two clips of the file. Closed as harmless after the fix; reopen if it is seen again.

#### 10. `libs/compositor`

Besides the deadlock above: software inputs reuse one staging buffer and image set per
input slot (`stage_upload`), and the render graph is validated in one pass over its
layers. Each render still creates and destroys a view per GPU plane and allocates its
binding map and semaphore lists (`vulkan_compositor.cpp:567`, `:806`, `:908`), so a
1080p60 viewer makes a few hundred Vulkan objects and small allocations per second.
*Measured (2026-10-15, temporary timers, release)*: the two views of a VA-API frame cost
~0.01 ms of a 2.6 ms render (1080p H.264, 4K HEVC): no pooling.

#### 11. `apps/omamovie` (first passes)

- [x] Build a clip's waveform only for its visible part. `WaveformItem` makes one bar per
  2 px of the whole clip (`waveform_item.cpp:115`) and the clip item is as wide as the clip
  (`TimelineView.qml:575`, up to 2,400 px/s), so a one-hour clip at full zoom asks for
  ~4.3 M bars, 26 M vertices (~200 MB) on each repaint, and every zoom step repaints.
  Derived, not observed. *Fixed (2026-10-14)*: items over 4,096 px build the bars within a
  window width of the screen (`waveform_span.cpp`) and only they bind the timeline's
  scroll, so narrow clips still never repaint on scroll; `tests/app/test_waveform_span.cpp`
  covers narrow, wide and off-screen items; GUI smoke passes; the 5,000-clip audit stays in
  its band (split p95 80.9 ms, undo 33.3 ms). Not measured: a long clip at full zoom.
- Logging: the app has one one-shot warning flag (`warned_silent_`) and no per-frame log
  on the playback or viewer paths, so §19's rate limiting needs no helper in `libs/base`
  yet (closes the open note in part 1).
- Shared worker: saves, autosaves, imports, thumbnails, scopes and previews share the
  one `workers_` thread (`session.hpp:821`); a save can wait behind an import's probe and
  thumbnail (hundreds of ms per file, 0.3-0.5 s for a 50-item reopen). Not a defect until
  a save is measured waiting; exports and paused seeks already have their own pools.

Continued in [part 5](implementation-plan-code-review-5.md).

[← Implementation plan](implementation-plan.md)
