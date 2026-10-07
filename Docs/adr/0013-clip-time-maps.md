# ADR-0013 — Clip time maps: freeze, reverse and speed ramps

**Status: Accepted (2026-10-05).** Complements ADR-0002, as its 2026-10-03 review note
requires before freeze, reverse or ramps exist.

## Context

ADR-0002 gives every clip a `TimeMap` from clip-local timeline time to media time and
expects frozen, reversed and ramped segments. Only constant speed exists
(`TimeMap::constant`). The review note fixed three points: a frozen segment has timeline
duration but no source displacement, so duration cannot be derived from the map alone;
reverse is decreasing within a segment; ramps integrate speed, and arbitrary curves would
need bounded approximation and explicit rounding. The model already stores the clip's
duration (M5 choice), and rounding to a media PTS happens only in `timeline::evaluate`.

## Decision

A `TimeMap` is either **constant** (today's map: one speed for the whole clip, whatever its
duration) or a list of **segments** that cover the clip exactly.

- A segment has an integer length in sequence ticks (> 0) and a speed shape:
  - **linear**: a constant rational speed, positive or negative (reverse), never zero;
  - **freeze**: speed 0, the media position held;
  - **ramp**: speed changing linearly from `from` to `to` across the segment. Both ends have
    the same sign or one of them is zero, so the segment is monotonic; both zero is a freeze.
- Segments are continuous by construction: each starts at the media position where the
  previous one ended. `source_in` is the media position at the clip's first tick.
- The media displacement after `t` ticks of a segment of length `L` is exact rational time:
  `tb·s·t` (linear), `0` (freeze), `tb·(from·t + (to − from)·t² / (2L))` (ramp), with `tb`
  the sequence timebase. Eased presets are several ramp segments (piecewise-linear speed),
  so no curve is sampled and nothing is approximated; rounding stays the floor to the
  media timebase in `evaluate`. Arithmetic overflow is an error, never a wrap.
- With segments, the clip's stored duration equals the sum of the segment lengths
  (checked by `Timeline::validate`). The media range a clip uses is the span between the
  smallest and largest position over the segment boundaries (monotonic segments), and it
  must lie inside the media, like today's `[source_in, source_end)`.
- Editing keeps the representation closed: `TimeMap::slice(from, to)` returns the map of a
  tick range (a ramp cut inside keeps its exact speeds at the cut), so split, trims and
  ripple operate on segmented clips without approximation. Changing a clip's constant
  speed replaces a segmented map with a constant one.
- Clip audio stays silent for any map other than constant 1× until time-stretching exists
  (current behaviour for other speeds); freeze is silent by definition.
- Persistence: a constant map keeps the `speed` field; a segmented map is a `time_map`
  array of `{kind, length, speed | from, to}` with rationals as strings. The format
  version increases with a migration that leaves older files unchanged, so an older
  OmaMovie refuses a project with segments instead of dropping them.

## Alternatives considered

- **Bézier or sampled speed curves.** Need numerical integration and inversion, so
  bounded approximation and a rounding policy per evaluation; piecewise-linear speed covers
  the ease-in/out presets the creator research asks for with exact integrals.
- **Reverse as a clip flag.** Cannot express "forward, then backward" boomerangs and
  duplicates what a negative-speed segment already says.
- **Derive duration from the map.** Impossible for freeze (zero displacement), as the
  review note says; the stored duration stays the source of truth.

## Consequences

- Timeline positions remain integer ticks; media positions remain exact until `evaluate`.
- Ramp numerators grow with `t²`; very long ramp segments can overflow 64 bits and are
  rejected with an error.
- Speed presets (M8) and their UI build on this; timeline audio for non-1× maps needs a
  separate time-stretching decision.
