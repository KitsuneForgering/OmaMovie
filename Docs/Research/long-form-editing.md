# Long-form editing: where size actually matters

> Documentary and source-code analysis, 2026-10-04. This is a research proposal, not
> a performance certification or a change to the native project-format ADR.

## Conclusion and decision

OmaMovie has no product reason to impose a short-video duration limit. A two-hour file
with three cuts and a two-hour project with thousands of clips stress different parts of
the system. Keep the versioned JSON project and simdjson parser for now. Complete export
and recovery, then measure timeline size, media seeking, preview, memory and export on
declared long-form workloads. Add proxies, preview renders, indexes or a different project
store only when a measured bottleneck justifies their cost.

The first long-form promise should be **a recoverable two-hour edit on the reference Intel
machine**, with the exact source codecs and scene complexity stated. It is not a claim that
all long or professional productions work. The gate below is proposed and has not run.

## Question, foundations and hypotheses

Question: which changes let OmaMovie edit and deliver longer material without losing work or
making common actions unresponsive? Baseline: current JSON/`simdjson` project, in-memory
timeline, Qt Quick shell, software-decoded viewer and no export. Failure costs include lost
edits and a final video whose image or sound differs from the preview.

The relevant foundation is a *quality scenario*: specify the workload and stimulus, then
measure the response rather than call an implementation “scalable” in isolation. The
[SEI scenario method](https://www.sei.cmu.edu/documents/704/2003_005_001_14213.pdf)
(Appendix A, pp. 43–44; 2003 technical report, accessed 2026-10-04) describes stimulus,
environment, response and response measure. Here the independent dimensions are source
duration, clip/track count, resolution/codec/GOP, effects, simultaneous streams, and output
duration. A parser benchmark measures only one dimension.

| Hypothesis | Mechanism and current evidence | What would change the decision |
|---|---|---|
| H1: simdjson is sufficient for the native project | `libs/project/src/document.cpp` reads a bounded 64 MiB JSON file into memory and parses it with simdjson DOM; the JSON contains references and edits, not video frames. ADR-0007 reports exact round trips but no large-project timing. | Real projects near the cap or measured load/save memory/latency dominate the workflow after simpler representation fixes. |
| H2: clip count will hurt the UI before duration alone | `Session::clips()` and `audioTracks()` build fresh `QVariantList`s of every clip; `Main.qml` uses several `Repeater`s over them. An edit copies the timeline snapshot and emits whole-sequence change signals. This is a source-level risk, not a benchmark. | Large-clip fixtures show stable UI time and memory, or profiling identifies a different dominant cost. |
| H3: random access and effects need distinct remedies | FFmpeg seeks to a keyframe and decoding reaches the requested frame. Kdenlive documents proxies for source decoding and preview renders for effect-heavy sections. OmaMovie's viewer currently uses software decode; its `FrameSource` has bounded decoder/history caches. | Matched footage shows proxy generation cost exceeds seek benefit, or effects are cheap enough without a preview cache. |
| H4: completion/recovery outweigh parser speed for long work | Atomic manual save exists, but autosave, relink UI and export remain open in M7. A long export has more opportunities for interruption and A/V drift. | Complete long-form workflow and failure-injection evidence show these paths already reliable. |

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

## Alternatives and bounded applications

| Problem observed | First candidate | Alternative and tradeoff |
|---|---|---|
| Large clip count slows edits or scroll | Retain sorted clip vectors; profile validation, snapshot copies and QML materialization. Pilot visible-range delegates/incremental model updates before altering timeline storage. | An ID/time index can speed lookup but adds mutation and consistency work. Qt documents [incremental models](https://doc.qt.io/qt-6/model-view-programming.html#performance-optimization-for-large-amounts-of-data) and [delegate reuse](https://doc.qt.io/qt-6/qml-qtquick-listview.html#reusing-items); their behavior is evidence of available mechanisms, not proof they fit this timeline. |
| Compressed long-GOP or high-resolution sources seek slowly | Pilot proxies with explicit original/proxy identity, measured generation/disk cost, bounded cache, missing-proxy fallback and original-media export. | An optimized edit-friendly source can help if proxy switches complicate quality checks. FFmpeg's [seek contract](https://ffmpeg.org/doxygen/8.1/group__lavf__decoding.html) is keyframe-based; actual delay is codec/GOP/device dependent. [Kdenlive proxy configuration](https://docs.kdenlive.org/en/getting_started/configure_kdenlive/configuration_proxy_clips.html) is a workflow example, not an OmaMovie speed result. |
| Effects, not decoding, miss preview deadlines | Cache rendered timeline *regions* keyed by source, edit revision, effects, color and preview settings; invalidate only affected ranges. | Source proxies alone retain the effects cost. Kdenlive explicitly distinguishes [timeline preview rendering](https://docs.kdenlive.org/en/tips_and_tricks/tips_and_tricks/timeline_preview_rendering.html) from proxies and says it improves playback, not edit operations. Cache stays disposable and export must be independently checked. |
| JSON load/save becomes costly | First remove avoidable copies, profile parsing vs DTO/restore/reimport, and test the 64 MiB cap against real generated projects. Keep JSON if acceptable. | SQLite as a project store offers transactions and indexed queries, but adds schema/migration, application-file and backup behavior. Its [application-file guidance](https://www.sqlite.org/appfileformat.html) supports it as an option; [WAL behavior](https://www.sqlite.org/wal.html) adds sidecars/checkpoints and network-filesystem limits. It is not an automatic upgrade from simdjson. SQLite remains plausible for a disposable media index. |
| Long export fails or drifts | Build a cancellable export job, write to a temporary destination, verify its decoded output and A/V sync before advertising completion; define failed-job cleanup/retry. | Segmenting or checkpointing an export can help after failures, but exact boundaries and joins add timestamp/GOP/codec complexity. FFmpeg's [segment muxer documentation](https://www.ffmpeg.org/ffmpeg-formats.html#segment-1) warns that split points depend on keyframes. Pilot only after one continuous export works. |

Kdenlive and Qt documentation describe mechanisms, not comparative measurements on the
OmaMovie machine. SQLite's authors describe database capabilities, not a requirement for
this application. These sources share their own product/institution lineages; they are not
independent evidence of OmaMovie performance.

## Proposed gate, before optimizations

Use a reproducible generated set plus real Omarchy/phone footage. Declare codec, GOP,
resolution, storage, driver, FFmpeg/Qt builds and project edit revision. Compare: (A) one
two-hour source with a few cuts, (B) two hours assembled from 5,000 short clips across
video/audio tracks, (C) a two-hour 4K long-GOP source with repeated random seeks, and
(D) a 10-minute section with costly effects. The 10-minute fixture is the control.
The 5,000-clip fixture is **synthetic stress**, not a claim about typical users.

1. Record cold/warm open, save and reopen time; peak RAM; project bytes; parser, DTO,
   validation, media reimport and UI-model phases separately. Require an exact project
   round trip and explicit failure when the format cap is exceeded. No parser/store change
   follows merely from total open time.
2. On the reference Intel machine, record p50/p95/p99 and maximum for split, trim,
   undo, timeline pan/zoom, selecting a clip, play start and random seek; track UI-thread
   stalls, decoder restarts, dropped frames, CPU/GPU/RAM and cache disk use. **Proposed
   interaction target:** p95 under 100 ms and no single routine edit stall above 250 ms
   in the 5,000-clip scenario. Revisit a threshold only as an explicit product decision,
   not after seeing a favorable run. Profile the worst path before adding an index.
3. Compare identical source/output settings for current viewer, proxy and/or preview
   render as each becomes available. Include generation time and disk budget. Accept a
   cache only if it improves the affected p95/p99 or drops without changing result,
   and deletion/failure of cached data leaves the original project usable.
4. Once export exists, run the full two-hour timeline with sound, effects and seeks.
   Check the output with an independent decode, sampled content at cuts/transition
   boundaries, audio continuity/sync at beginning/middle/end, cancellation and disk-full
   recovery. A complete file plus `ffprobe` metadata alone is insufficient.
5. Force a crash or power-loss surrogate around save and autosave on the supported local
   filesystem; verify recovery of the last acknowledged save and the documented autosave
   state. Test missing/moved media and LUTs, relink, and a failed proxy cache.

**Stop conditions:** no release claim for a workload that loses acknowledged edits,
silently changes output, uses unbounded memory/disk, or cannot produce a verified movie.
For claims of smooth playback, apply M4's frame-time/A/V criteria to the declared scene.

## Research record and limits

Budget: one local code pass over project, timeline, viewer, waveform and QML paths;
four targeted online search passes covering FFmpeg, Qt, Kdenlive, SQLite and the SEI;
one arithmetic check. Stopped because the next decision is a workload benchmark, not another feature
list. No long-form runtime benchmark, user study, export or failure-injection test ran.
The arithmetic was executed locally with Python on 2026-10-04; its two-hour result is
`7200 × 100 × 2 × 4 / 2^20 = 5.49 MiB`. Online sources were inspected as HTML on
2026-10-04 except the SEI PDF, whose relevant Appendix A passage was available in text.
The cited FFmpeg API is 8.1 documentation; Qt pages are 6.12 unless their URL says
otherwise; Kdenlive pages are its 26.08 manual. Page update dates not shown are unknown.
The main uncertainty is the workload distribution and measured bottleneck on OmaMovie's
reference hardware.
