# OmaMovie on FilmCraft's benchmark scenarios (2026-10-14)

FilmCraft's `cargo xtask bench` sections, rerun with OmaMovie's own audits on FilmCraft's
fixture recipes. Raw output, machine and commands:
[evidence](../Research/evidence/2026-10-14-filmcraft-equivalent-benchmarks.txt). Intel
i5-1135G7 (4 cores), Iris Xe, load 2–7. FilmCraft was **not** run here: its figures come from
its `docs/performance.md` (Apple M4 Pro at load 75–200, and an i5-13500H for VA-API), so each
row compares two machines and two measurement methods. Read it as orientation, not a ranking.

## Decode (FFmpeg, every frame; FilmCraft decodes with its own Rust decoders)

| codec | size | CPU ms/frame sw → VA-API | fps sw → VA-API | FilmCraft Linux sw → VA-API (ms) |
|---|---|---|---|---|
| H.264 | 1080p | 30.7 → 0.8 | 215 → 444 | 57–104 → 2.3–2.6 |
| H.264 | 2160p | 120 → 2.4 | 51 → 118 | 232–417 → 11–13 |
| HEVC | 2160p | 78 → 2.4 | 62 → 286 | 203–350 → 10.1 |
| HEVC Main 10 | 2160p | 100 → 3.0 | 50 → 247 | 212–349 → 18.4 |
| VP9 / AV1 | 2160p | 46 / 18 → 1.3 / 1.1 | 93 / 153 → 298 / 285 | software only |
| ProRes 422 HQ | 2160p | 128 (no VA-API) | 45 | 87–89 (M4 Pro) |

The VA-API rows keep frames on the GPU; FilmCraft's include a `vaGetImage` read-back, which its
document names as the 4K bound. This is the decoder alone, not OmaMovie's import path.

## Playback in the app (`--m4-audit`, 8 s, 192 frames due)

Every file passed A/V sync on both paths: 189–192 composites with VA-API to Vulkan, 172–188 in
software, 0–5 dropped. Process CPU per played frame:

| file | VA-API | software |
|---|---|---|
| H.264 1080p | 6.5 ms (0.16 cores) | 31 ms (0.74) |
| H.264 2160p (~170 Mbit/s) | 7.3 ms (0.18) | 230 ms (5.5) |
| HEVC / VP9 / AV1 2160p | 5.6 / 7.1 / 5.3 ms | 89 / 28 / 33 ms |

FilmCraft's 2160p H.264 Full play on the M4 Pro went from 0/192 to 192/0 shown depending on load,
at 87–183 ms CPU per frame. Composite cost here: 3.6 ms (1080p HW), 11 ms (2160p HW), 15–17 ms
(2160p software, upload included).

## Seek and scrub (24 seeks)

Playing: first frame p50 93–259 ms, UI blocked p50 3–9 ms with VA-API but 71–77 ms in software
at 2160p. Paused (scrub): VA-API freezes the UI event loop p50 66 ms (1080p), 132 ms (HEVC 4K),
**331 ms, p95 1.3 s** (H.264 4K); software stays at 4–9 ms. This is the paused-hardware-seek
problem in [m11-paused-hardware-seek-note.md](m11-paused-hardware-seek-note.md), now measured at 4K.

## Export, timeline, project

- Export of the 20 s 1080p H.264 + AAC source: 63–65 fps with VA-API H.264, 27 s process CPU
  (~56 ms per frame, startup included), 359 MB peak. FilmCraft: 10–12 fps, ~250 ms CPU per frame
  with its CPU encoder on the M4 Pro.
- `--m6-timeline-audit`: 1000 clips open (save, load, reimport, UI) in 95 ms, edits p95 ≤ 53 ms;
  5000 clips in 194 ms, split/ripple/undo p95 108–136 ms, 581 MB peak. FilmCraft times one egui
  frame (~3 ms at 1000 clips) and save/open of 5000 clips (~20 ms): not the same quantities.

## Not covered

`stack3` and blend-mode playback (no audit builds a multitrack project), rendered previews and
per-thread CPU columns. A same-machine comparison still needs FilmCraft built and run here.

[← FilmCraft comparison](omamovie-2026-10-12-filmcraft.md)
