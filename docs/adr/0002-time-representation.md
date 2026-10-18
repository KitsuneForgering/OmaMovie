# ADR-0002 — Time representation

- **Status:** Accepted (2026-10-02)
- **Milestone:** M0 (base types); M5 (clip time mapping)

## Evidence review (2026-10-03)

The rational-time decision remains accepted. The exact NTSC rate is 30000/1001, whereas
29.97 is a decimal approximation of it; binary floating point alone does not imply practical
drift for every algorithm. Exact representation/explicit rounding are the actual design
reasons. CapCut's documented curves support a feature candidate, not measured audience demand.

Before M5, complement the TimeMap decision with domain/interpolation rules: a frozen segment
has nonzero timeline duration despite zero source displacement; reverse is decreasing within
a segment; ramps integrate speed. Arbitrary curve samples need bounded approximation and
explicit rounding. The historical statement that duration is always derived from the map
must not be implemented from source displacement alone. See the [audit checks](../Research/skeptical-review.md#quantitative-checks).
No TimeMap implementation or accepted decision is changed by this note.

## Context

`double` timestamps accumulate error, cannot represent 29.97 fps exactly and break
equality comparisons. Real media is often VFR (phones, screen recordings) and mixes
timebases (90 kHz in MPEG-TS, 1/30000 in MP4, audio samples at 44.1/48 kHz). Product
research (`docs/Research/capcut.md` (historical research reference; see the dated review note)) showed speed ramps are a creator requirement,
which means mapping timeline time to media time through a curve, not only a constant speed.

## Decision

[Read the decision](0002-decision.md)

## Alternatives considered
- **`double` seconds**: simple, but imprecise and without reliable equality.
- **Integer in a fixed global timebase (e.g. 1/705600000, the "flick")**: represents common
  rates without error, but not arbitrary file timebases, and still needs conversion at the
  boundary. May be reconsidered as the timeline's internal timebase.
- **`std::chrono::duration<int64, std::ratio<...>>`**: the ratio is fixed at compile time;
  media timebases are only known at runtime.

## Consequences
- All time code handles `Result` and chooses a rounding mode: more verbose, but precision
  loss points are visible in the code.
- Tests cover 23.976/29.97/59.94 fps, 44.1/48/96 kHz, ten hours at 90 kHz, rounding of
  negative values, ties and overflow (`tests/base/test_rational.cpp`, `test_time.cpp`).
- `TimeMap` with curves still needs a complementary ADR in M5 for interpolation (curve
  choice and numeric evaluation).
