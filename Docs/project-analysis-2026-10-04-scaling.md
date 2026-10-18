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

[Return to project-analysis-2026-10-04.md](project-analysis-2026-10-04.md)
