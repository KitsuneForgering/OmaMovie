## Local code observations and capacity check

* `project::load` allocates the whole JSON text and the simdjson DOM, then builds a
  `Document` and validated `Timeline` (`libs/project/src/document.cpp:822,1061`). The 64 MiB
  input cap limits hostile input; it is **not** a useful-video-duration limit. Replacing
  the parser does not remove DOM, DTO, timeline or QML copies.
* `Timeline::clip_at` binary-searches clips ordered by start (`libs/timeline/src/model.cpp:139`),
  while `evaluate` visits every track and looks up media by linear search
  (`libs/timeline/src/evaluate.cpp:185`). `Editor::execute` validates the whole timeline
  after each edit (`libs/timeline/src/editor.cpp:9`). Those choices may be fine for small
  projects; profile them before adding indexes with update/invalidation costs.
* `Session::afterEdit` makes a new full timeline snapshot and emits `sequenceChanged`
  (`apps/omamovie/src/session.cpp:376,1689`). `Session::clips` and `audioTracks` materialize
  all clips (`:1473`); multiple QML `Repeater`s instantiate delegates across the sequence
  (`apps/omamovie/qml/Main.qml:1504,1611,1844`). This is a concrete scaling candidate
  when clip count rises, independent of source duration.
* `VideoScheduler::start` recreates its `FrameSource`, and `stop` waits for the worker
  (`apps/omamovie/src/video_scheduler.cpp:10,50`). `FrameSource` chooses forward decode or
  seek and bounds its frame history (`apps/omamovie/src/frame_source.cpp:17,63,93`). Measure
  rapid seeks and start/stop latency before changing decoder ownership.
* Waveform generation decodes the entire audio source at 100 buckets/s
  (`libs/playback/src/waveform.cpp:65`; `waveform.hpp:40`). **Local arithmetic, not a memory
  measurement:** two float arrays (peak and RMS) for one two-hour source occupy about
  5.49 MiB at that density; 24 hours occupy about 65.92 MiB, excluding vector capacity,
  metadata and duplicate sources. Formula: `seconds × 100 × 2 × sizeof(float)`.

[Return to long-form-editing.md](long-form-editing.md)
