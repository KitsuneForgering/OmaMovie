# OmaMovie project analysis — 2026-10-04

## Decision

Finish and verify the **import → edit → save → reopen → export** path before adding more
effects, interchange formats or extension mechanisms. The existing time model, command
based editing and GPU compositor are useful foundations. The main product gap is that a
user still cannot deliver a movie from the app. The next performance work should measure
the complete viewer and large timeline interactions; the current spikes do not establish
either claim.

This is a source and document review of commit `1654e1b` plus an uncommitted working tree
on 2026-10-04. Several implementation and evidence files were already modified before
this review; their changes were not audited as a final revision. Inspected paths:
`apps/omamovie` session, viewer, scheduler and QML; `libs/timeline`, `libs/playback`,
`libs/project`; the implementation plan and recent research/evidence. I did not rerun
tests, GUI playback, benchmarks or failure injection. The 2026-10-04 `make test` result
and S5 timings cited below are [existing records](Research/evidence/2026-10-04-verification.md),
not new observations in this review.

## Project model and sound choices

OmaMovie is a local Qt Quick video editor for Omarchy, aimed first at recordings and
phone footage. The primary data path is media probe/decode → rational timeline evaluation
→ Vulkan composition → Qt preview, with separate timeline audio mixed to PipeWire.
`Session` connects the UI to these modules. A versioned JSON document saves references
and edits. Export is still a planned path ([M7](implementation-plan.md#m7--project-and-export)).
Workload dimensions are source duration, clip count, track count, distinct media, codec/GOP,
resolution, active layers/effects and output duration; none can be replaced by one “project
size” number.

Worth preserving:

- Rational time and a sorted, nonoverlapping clip vector give exact edit boundaries and
  logarithmic clip lookup within a track ([model.cpp](../libs/timeline/src/model.cpp)).
- Commands validate the complete timeline and revert failed edits; the cost buys a strong
  current correctness boundary ([editor.cpp](../libs/timeline/src/editor.cpp)).
- The video producer has a bounded queue and drops stale prepared frames against the clock
  ([video_scheduler.cpp](../apps/omamovie/src/video_scheduler.cpp)). The frame source bounds
  decoder count and retained history ([frame_source.cpp](../apps/omamovie/src/frame_source.cpp)).
- Project input has file, depth and item limits, and saving writes a same-directory temporary
  file, syncs it, renames it and syncs the directory ([document.cpp](../libs/project/src/document.cpp)).
  That is a sound design; crash durability still needs the planned failure test.

## Findings and priorities

| Priority | Finding and evidence | User effect / action | Proof of success |
|---|---|---|---|
| High | **Delivery path incomplete.** Export action is disabled ([Main.qml](../apps/omamovie/qml/Main.qml)); M7 lists encoding, muxing and output validation as open. | A finished edit cannot become a movie. Implement a narrow SDR H.264/AAC export using the existing timeline evaluation, with software fallback and a temporary output. Verify decoded picture at cuts/effects and A/V sync at start, middle and end. | A saved project reopens and exports a specified short fixture; independent decode, cancellation and disk-full checks pass. |
| High | **Recovery remains incomplete.** Manual save is atomic in source, but autosave, prior-version recovery and save failure injection remain open in M7. The UI's discard dialog only offers discard/cancel ([Main.qml](../apps/omamovie/qml/Main.qml)). | An interrupted session or mistaken edit can lose work. Add recoverable autosave after the basic save/export path, then test failure at write, file sync, rename and directory sync. | Last acknowledged save remains loadable; the UI can restore the intended autosave/prior version after a forced interruption. |
| High | **Playback claim needs an end-to-end gate.** `FrameSource` forces software decode for the current viewer ([frame_source.cpp](../apps/omamovie/src/frame_source.cpp)); the Qt presentation path is separate from the S5 decode/composite harness. S5 reported 3–4 of 600 iterations above 16.67 ms even before Qt presentation ([verification](Research/evidence/2026-10-04-verification.md)). | Test a named 1080p30/60 scene at actual Qt presentation with negotiated decode path, frame age, repeats/drops, p95/p99, A/V drift and memory. Admit hardware decode only after lifetime/queue validation. | Sustained run meets the predeclared M4 criteria on the named Intel configuration; no broader hardware claim follows. |
| Medium, measured gate first | **Audio mixing scans every clip for each output block.** `TimelineAudio::render` traverses all tracks/clips and calls linear `find_media` before the time overlap test ([timeline_audio.cpp](../libs/playback/src/timeline_audio.cpp), [model.cpp](../libs/timeline/src/model.cpp)). | At thousands of clips, fixed-size audio blocks may spend time on unrelated edits. Measure callback work/underruns for the 5,000-clip fixture. If material, start at the first overlapping clip per sorted track and stop after the block, retaining transition lead/tail. | Lower p95/p99 mix time and no changed samples at boundaries; no new underruns. |
| Medium, measured gate first | **UI edits rematerialize the sequence.** Every successful command validates and copies a full timeline snapshot, then emits whole-sequence change signals ([editor.cpp](../libs/timeline/src/editor.cpp), [session.cpp](../apps/omamovie/src/session.cpp)); `clips()` and `audioTracks()` rebuild variant lists, and QML uses repeaters ([Main.qml](../apps/omamovie/qml/Main.qml)). | Large clip counts may stall editing and scrolling. Profile validation, copy, conversion and delegate creation separately before changing storage. First pilot visible-range delegates or incremental Qt models if the UI dominates. | The declared 5,000-clip interaction gate meets its p95 and worst-stall targets without undo/drag regressions. |
| Medium | **Save/open behavior needs state-machine tests.** `saveProject` captures a timeline copy and queues work, while `openProject` restores the timeline and reimports media asynchronously ([session.cpp](../apps/omamovie/src/session.cpp)). | Test edits during save, rapid repeated saves, opening while an import is pending, missing media/LUTs and close during save. A successful write must identify the revision actually saved, and reopening must keep missing references visible. | GUI tests observe correct dirty state, project path, warnings and subsequent reopen under each sequence. |

The first three rows are required capabilities or correctness gates. The audio/UI rows are
**code-derived scaling risks**, not measured bottlenecks. The save/open row is a test gap,
not a confirmed data-loss defect. The existing long-form proposal already defines useful
fixtures and interaction thresholds ([long-form research](Research/long-form-editing.md));
reuse those rather than inventing another benchmark.

## Scaling model

Let `T` be tracks, `C` total clips, `C_t` clips on track `t`, `M` distinct media entries,
`A` active clips at one video instant, `B` audio output blocks, and `F` output frames.
These are source-derived bounds; decoder, shader, Qt and disk costs remain workload-dependent.

| Operation | Current work | Extra/retained space | Basis and limit |
|---|---|---|---|
| One video timeline evaluation | `O(T² + Σ log C_t + A·M)` plus property/transition work | `O(A)` output layers | Each track calls `clip_at`, which finds the track linearly, then binary-searches its clips; active media lookup is linear ([evaluate.cpp](../libs/timeline/src/evaluate.cpp), [model.cpp](../libs/timeline/src/model.cpp)). Usually `T` is small. |
| One edit | Up to `O(C·M + C·L + C)` validation for `L` LUTs, then `O(C)` timeline copy and UI materialization, excluding effect/curve payloads | `O(C)` snapshot and variant/delegate state | Every command calls `validate`; each clip finds its media and possibly LUT by linear scan ([editor.cpp](../libs/timeline/src/editor.cpp), [model.cpp](../libs/timeline/src/model.cpp), [session.cpp](../apps/omamovie/src/session.cpp)). |
| Audio over `B` blocks | `O(B·C·M + decoded/mixed samples)` in the worst case | Active decoders and one scratch buffer | Each block scans clips and looks up media before rejecting inactive spans ([timeline_audio.cpp](../libs/playback/src/timeline_audio.cpp)). |
| Viewer over `F` frames | `F` evaluations plus decode, composition and presentation | Bounded frame queue/history plus decoded/composited images | A bounded queue controls in-flight frames, but full-frame GPU memory and driver allocations need measurement ([video_scheduler.cpp](../apps/omamovie/src/video_scheduler.cpp), [frame_source.cpp](../apps/omamovie/src/frame_source.cpp)). |
| Project load/save | At least `O(J + C)` for JSON bytes `J` and `C` clips; load also performs timeline validation as above | `O(J + C)` plus nested effect data and copied timeline data | File is capped at 64 MiB; parser, DTO, restored model and UI are distinct costs ([document.cpp](../libs/project/src/document.cpp)). |

These bounds identify where to instrument, not a reason to replace vectors or JSON now.
An index or cache adds mutation, invalidation and memory costs. The sorted vector and bounded
queues fit the present workload until representative profiling shows otherwise.

## Architecture and product decision

The module boundaries are mostly appropriate for a desktop editor: Qt stays in the app,
and the timeline model does not depend on FFmpeg or Vulkan. The remaining architecture
work should serve concrete outputs: one shared, deterministic composition contract for
preview and export; explicit output color/audio settings; lifecycle rules across Qt,
decoder and compositor; and recoverable project state. A separate service, general plugin
API, alternate project database or importer framework would add maintenance before this
workflow is proven.

The relevant engineering lenses are timeline invariants and algorithmic complexity for
large edits; real-time scheduling/backpressure for audio and preview; durability and
recovery for projects; and end-to-end quality scenarios for the editor's user-visible
claims. The current tests cover many library invariants, but a green library suite cannot
establish delivered movie correctness, crash recovery or smooth Qt presentation. Those
are the next integration gates.

Recommended order: (1) complete narrow export and independent output checks; (2) finish
save/reopen recovery with fault injection and state-machine tests; (3) run the M4 viewer
gate and long-form interaction gate, then optimize only the measured dominant paths.
The cost is a slower feature rollout, but it yields a usable editing loop and avoids
optimizing or expanding an unverified one.
