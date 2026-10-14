## Evidence status (2026-10-14)

Intel i5-1135G7 (4 cores / 8 threads), Iris Xe (Tiger Lake), 15 GB, Mesa 26.2.2, iHD 26.2.4.
FilmCraft's fixture recipes ([evidence](Research/evidence/2026-10-14-filmcraft-equivalent-benchmarks.txt),
[note](analysis/omamovie-2026-10-14-filmcraft-benchmarks.md)). *Implemented* = code exists;
*measured* = numbers on this machine; *validated* = passed a predeclared gate; *hypothesis*
= not yet measured.

| Area | Status | Evidence and limits |
|---|---|---|
| VA-API decode, FilmCraft fixtures | Measured | H.264 1080p 444 fps / 0.8 ms CPU per frame, 2160p 118 fps / 2.4 ms; HEVC 1080p 1121, 2160p 286 / 2.4 ms, Main 10 2160p 247 / 3.0 ms; VP9 1212 / 298; AV1 1101 / 285 fps. FFmpeg decode to the VA-API surface only: no import, composite or presentation |
| 4K playback, whole app (hardware) | Measured, one machine | H.264, HEVC, VP9 and AV1 2160p at 23.976 fps: 189–192 of 192 frames composited in 8 s, A/V audit PASS; composite p50 11.2 / 10.7 / 10.6 / 11.2 ms against a 41.71 ms frame; 5–7 ms process CPU per frame |
| GPU-first pipeline | Partially validated | No longer limited to decode/compositor microbenchmarks or 1080p60: the full app plays 4K24 for four codecs on modest hardware. Not validated: 4K60, multicam, heavy effects, complex HDR, AMD, NVIDIA |
| Paused hardware scrub | **UI freeze fixed; first-frame latency on long GOPs is open debt** | UI event-loop gap p50 / p95: H.264 1080p 66 / 270 ms; H.264 4K 331 / 1323 ms (max 1481); HEVC 4K 132 / 253 ms. Software scrub: 4–9 ms p50. Cause located (decode in Qt's sync phase). *UI gap fixed 2026-10-14* (p95 12–19 ms, [ADR-0020](adr/0020-async-execution-boundary.md)); first-frame latency on long GOPs unchanged |
| 5,000-clip structural edits | **Measured; gate passes after a fix (2026-10-14)** | Was split p95 136–147, ripple 108, undo 126–131 ms, RSS ~600 MB: not a code regression (2026-10-07 code gives the same today) but the QML garbage collector freeing whole clip lists that nine menu bindings read for `.length`. With `Session::clipCount`: split 84–93, ripple 33–43, undo 33 ms, RSS ~390 MB ([evidence](Research/evidence/2026-10-14-timeline-5000-stalls.txt)). One ~90 ms split sample per run is unexplained. Trim drag p95 17.0 ms |
| Export 1080p H.264 + AAC | Measured, functionally strong | 62–65 fps; per frame 7.3 ms render + readback, 6.7 ms VA-API encode, 1.1 ms audio; 355–359 MB peak. Render → readback → encoder is a profiling candidate, not a change: no gate is blocked by it |

### Corrected statements

- "The measured 5,000-clip UI path does not justify a rewrite" stays true of the *design*, but
  the 2026-10-06 numbers no longer describe the build. Hypothesis: the gesture path is healthy
  while structural operations plus model/UI update exceed the budget again. Do not assume a
  core algorithmic regression; measure first, per edit: command/domain cost, model mutation,
  reattachment, validation, `QAbstractItemModel` notifications, QML delegate work and the
  first rendered Qt frame. *Measured 2026-10-14*: the hypothesis was wrong. Command, model and
  first frame fit (17–45 ms); the p95 was the QML garbage collector freeing whole clip lists read
  by bindings that needed only a count, fixed with `Session::clipCount`.
- "Hardware decode is the better path" holds for throughput (playback) only. Throughput and
  interactive random access have different goals and need not share a scheduling policy. The
  scrub numbers do not argue for software decode; the hypothesis is that the hardware path's
  synchronization, ownership or blocking work does not fit the latest-request-wins semantics
  scrubbing needs.

### Permanent FilmCraft-equivalent workload

A fixed external workload limits benchmark gaming; it is not a race with FilmCraft or
Premiere. Planned as `tools/bench/filmcraft-equivalent/` (the `tools/bench` slot of
CLAUDE.md §23), one script per section driving existing audits: `fixtures` (FilmCraft's
recipes, generated, never committed), `decode` (FFmpeg software/VA-API), `playback`
(`--m4-audit`, hardware/software), `seek` (`--m4-seek-audit`), `scrub`
(`--m4-scrub-audit`), `export` (`--export-audit`) and `timeline` (`--m6-timeline-audit`).
It records machine, driver, FFmpeg version, decode path and load average with every result.
Missing: a multitrack scene (FilmCraft's `stack3`), which needs an audit that builds one.

[← Implementation plan](implementation-plan.md)
