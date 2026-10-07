# ADR-0014 — Connected clips: anchors to a primary clip's content

**Status: Accepted (2026-10-05).** Settles the M5 connected-clip contract before the native
format is frozen ([Final Cut research](../Research/final-cut-pro.md)); the maintainer chose
anchors over a documented grouping policy.

## Context

The storyline ripples on its own track only. Titles, cutaways and detached voice-overs on
other tracks therefore drift when the storyline is edited. Final Cut keeps such clips
"connected" to a primary clip. The research asked for an explicit, persisted relation whose
behaviour under split, trim, ripple, delete, speed change and moves is written down, with
each edit one undo entry and no dangling anchor surviving any edit.

## Decision

A clip may carry `anchor = {primary, source}`: the ID of a primary clip on another track
and a **source position of the primary** (exact `RationalTime`, any timebase). The dependent's
first instant is attached to the instant where the primary shows that source position.

- **Attach instant.** For a primary `P` with constant speed `s`:
  `P.start + ceil((source − P.source_in) / (tb · s))` in sequence ticks; `source` lies in
  `[P.source_in, source_end(P))`. Connected clips follow the primary's **content**: moves,
  ripple shifts, trims and slips of the primary keep the dependent on the same frame of it.
- **Invariants** (`Timeline::validate`): the primary exists, is on another track, is not
  itself anchored (one level, like a storyline and its connected clips), has a constant
  time map, contains `source`, and the dependent starts exactly at the attach instant.
- **After every edit** a single pass (in the planned-command mechanism, so its changes are
  part of the same undo entry) re-attaches dependents:
  - the dependent was itself changed by the edit (the user moved or trimmed it): its anchor
    is recomputed from its new start if that start still lies on the primary, else dropped;
  - the primary still contains `source`: the dependent moves to the attach instant;
  - `source` now lies in a clip created by the edit from the primary (the tail of a split,
    same track and media): the anchor moves to that clip;
  - the primary was removed: the dependent is removed too (Final Cut's behaviour; undo
    restores both);
  - the primary survives but `source` was trimmed away, or its map became segmented
    (freeze, reverse, ramps): the anchor is dropped and the dependent stays where it is.
  If moving a dependent would overlap a clip on its track, the whole edit fails and leaves
  the timeline and history untouched.
- **Commands:** `connect(dependent, primary)` (the dependent's start must lie on the primary)
  and `disconnect(dependent)`. Edits stay generic; the UI decides when to connect.
- **Persistence:** a clip's optional `anchor` object `{primary, source}` in native format 2
  (introduced with ADR-0013 in the same unreleased version).

## Alternatives considered

- **Offset from the primary's start in sequence ticks.** Simpler, but a start trim moves the
  dependent off the content it belonged to; per-edit compensation would recreate the
  content model with more special cases.
- **Manual grouping (no relation).** No persisted relation means undo, save/load and edits
  outside the UI break it; kept as the comparison baseline for the M6 task study.
- **Nested anchors and anchoring to segmented primaries.** Need an inverse of non-monotonic
  maps and transitive moves; deferred until a task needs them.

## Consequences

- Each edit pays one pass over the clips (O(clips)); no index is kept until the long-form
  gate shows it matters.
- Removing a primary removes its dependents. The UI must show connections so this is not a
  surprise; the M6 comparison against manual grouping decides whether anchors stay.
- There are no locked tracks yet; when they exist, an edit that would move a dependent on a
  locked track fails like an overlap.
