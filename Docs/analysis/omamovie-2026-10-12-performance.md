## Performance evidence and unmeasured paths

The table reports historical project measurements, not new runs in this audit.
Use the linked evidence for hardware, build, fixtures, repetitions and cache state;
where a field is missing, it remains unknown rather than inferred.

| Path | Before → after / workload | Evidence and remaining limit |
|---|---|---|
| Timeline UI materialization | 500 clips: ~200 ms QML rebuild → 5,000 clips p95 ≤31 ms fitted, ≤60 ms working zoom | [M6 run](../Research/evidence/2026-10-06-m6-long-timeline.txt); different workload sizes, not a same-input speedup ratio |
| Trim drag preview | 5,000 clips: 97 → 25 simulated previews; p95 26.5 → 23.8–27.0 ms, max 161.9 → 147.6–195.3 ms | [drag run](../Research/evidence/2026-10-12-m6-trim-drag.txt); warm, one gesture; frame-side stall unexplained |
| Recordings scan | 10,000 local files warm: p50 33.39 → 14.85 ms | [scan run](../Research/evidence/2026-10-12-m6-recordings-scan.txt); cold/network state not measured |
| Project JSON | 5,000 clips warm: open 35 ms, save 22 ms, 12 MB file, 107 MB peak | [project run](../Research/evidence/2026-10-07-m7-project-open-save.txt); app reimport/cold open excluded |
| Playback seek | 60 random seeks: UI p50 16 → ~3 ms after async audio flush; first frame ~77–79 ms | [M4 run](../Research/evidence/2026-10-05-m4-av-drift.txt); 1080p30 H.264, one machine |
| Export | 1080p30 ~19.6 → ~52 fps after RGBA8 readback path; hardware encoder ~68 fps | [M7 run](../Research/evidence/2026-10-07-m7-export.txt); quality/file size differ, long 1080p not measured |
| Audio clip discovery | Sorted-track binary search changed derived O(clips × media) per block to O(log clips + active clips), excluding decode/effects | `libs/playback/src/timeline_audio.cpp:69`; no block-deadline before/after benchmark |
| Thumbnail/cache | Worker and bounded disk cache implemented | `apps/omamovie/src/session.cpp`, `libs/base/src/disk_cache.cpp`; hit rate and generation latency not isolated |
| Decode/compositing | Intel S5 measures two 1080p60 streams and compositing | [S5 evidence](../Research/evidence/2026-10-04-s5-minimal-compositing.txt); presentation and other devices not included |

For a proposed optimization, record baseline and candidate on the same hardware,
dataset, build mode, codec/decode path and warm/cold state. Core timeline lookup uses
sorted clips and binary search; whole-model validation and some anchor/media lookups
can cost more with clip count, while history memory has no global cap. A 10,000-clip
multilane edit/undo/load profile must identify the dominant phase before an index,
COW snapshot or project database is justified. Keep correctness checks separate from
timing thresholds; use broad regression bounds only for gross slowdowns.

[← Assessment](omamovie-2026-10-12.md)
