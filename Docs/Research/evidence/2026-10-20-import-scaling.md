# Timeline import cost by size — 2026-10-20

**Classification:** PERFORMANCE / algorithmic. Same machine (i5-1135G7, `intel_pstate`
`powersave`, EPP `balance_performance`, AC), Release build, one run per size.

**Workload:** `tools/bench/import-scaling/run.py build/release/tools/oma-project/oma-project`:
one-track OTIO timelines of N cuts of 12 frames at 24 fps over a one-minute file, written by
OpenTimelineIO 0.18.1 and converted to an OmaMovie project; every run must report `saved:`.

| Clips | Before (`2f7cdab`) | Batched validation | + sorted searches |
|---:|---:|---:|---:|
| 1,000 | 0.37 s | 0.06 s | |
| 2,000 | 0.97 s | 0.09 s | |
| 4,000 | 3.76 s | 0.16 s | |
| 8,000 | 16.1 s | 0.28 s | 0.10 s |
| 16,000 | > 120 s (still running at a 120 s timeout; finished, saved) | 1.10 s | 0.19 s |
| 40,000 | | 15.6 s | 0.46 s |

**Cause:** `project::from_ir` placed each clip with its own `Editor::execute`, and every execute
validates the whole timeline (`Timeline::validate`, hash maps over all media, tracks and clips).
n clips cost n validations of up to n clips: O(n²). With validation skipped as an experiment,
8,000 clips took 0.53 s, which attributed the time.

**Change:** the clips of a track run as one `edit::transaction` (one validation), then the
dissolves into them as another. A group that fails is halved until each edit lands or fails
alone, so the result per clip, and its report note, is what executing it alone gives
(`from_ir_batch.cpp`; `tests/project/test_from_ir_batch.cpp` places 300 clips with one past
its media and checks the other 299 at exact ticks; making a failed group drop all its clips
fails that test and no other). Positions are unchanged: `tools/interop/otio_diff.py` 500/500
seeds, and 500/500 with `--mixed`.

**Second stage (same day).** Batching left a quadratic term (16,000 → 40,000 clips: 1.1 →
15.6 s); `oma-project import` (parse only) stayed under 0.2 s and reloading the saved project
under 0.2 s, so it was in the model's edits. Three per-insert scans over every clip, each
measured by removing it in turn at 40,000 clips:
- `clear_range` (overwrite) walked the whole track: it now starts at the first clip ending after
  the range (binary search; clips are sorted and never overlap, so their ends are sorted too):
  15.6 → 9.4 s.
- `range_free` (every insert and replace) walked the whole track: same search, 9.4 → 3.6 s.
- `InsertClip` searched every track for the new clip's ID. The search stays (a duplicate on the
  same track would be erased with the original if a later step of the transaction failed), but
  the timeline now keeps an upper bound of clip IDs in use, which only grows; an ID above it
  skips the search. A freshly allocated ID is above it unless a caller once chose a larger ID
  itself (then the search runs, as before): 3.6 → 0.46 s.
`tests/timeline/test_clip_ids.cpp` covers duplicate IDs across tracks, after undo, chosen by the
caller and inside a failing transaction on the original's track (dropping the bound update makes
it fail), and moves at either edge and touching a neighbour (searching from the first clip
ending *at* the start instead of after it crashes the timeline suite).

**Measurement mistake corrected:** a first timing showed 0.07-0.28 s at every size before the
change because `convert` refuses to overwrite its output and the runs were refusals; the
benchmark now fails unless each run saved.

**Not done:** An import with many clips that cannot be placed still
pays one validation per failure. Each interactive edit still validates the whole timeline
(about 2 ms at 4,000 clips, derived from the first table), and `find_clip`, `track_of` and
locating a clip for replace or erase remain linear scans; the sorted searches above also apply
to interactive overwrites, inserts and moves, but no interactive latency was re-measured.
