# ADR-0011 — Transitions between clips

- **Status:** Accepted (2026-10-03)
- **Milestone:** ahead of M8 (maintainer choice, see the implementation plan)

## Context

Transitions blend two clips across a cut. Editors model them in two ways: as overlapping
clips (the outgoing clip extended under the incoming one, changing positions and the timeline
duration) or as an object at the edit point that borrows media past each clip's edge
("handles") while both clips keep their positions (Final Cut, iMovie). The timeline model
already guarantees non-overlapping clips per track (`Timeline::validate`), and the storyline is
magnetic: edits ripple positions all the time.

## Decision

- A transition belongs to the **incoming clip**: `Clip::transition_in` with a kind (dissolve,
  dip to black, wipe) and a duration in sequence time. It blends from whatever clip ends
  exactly where this one starts on the same track.
- It is **centered on the cut** and borrows media from both clips; positions and the timeline
  duration do not change. Its playable half length is computed, not stored:
  `timeline::transition_window` = min(duration / 2, media the outgoing clip has after its out
  point, media the incoming clip has before its in point, both clip lengths), exact in
  rational time. Stills have unlimited media.
- A transition whose clips no longer touch, or whose clips have no media to spare, is a
  **dormant plain cut**, not an invalid timeline. Edits never fail because of a transition, and
  undoing them brings it back. Splitting a clip gives the new right part no transition.
- Evaluation emits both clips inside the window (outgoing first) with an opacity factor and a
  reveal fraction (wipe); the compositor gained a per-layer reveal mask. Sound crossfades over
  the same window with equal-power ramps (`audio::ClipGain::lead/tail`).

## Alternatives

- **Overlapping clips** (the outgoing clip extends under the incoming one): breaks the
  no-overlap invariant every track relies on, changes the duration and positions, and makes
  ripple edits move transitions in surprising ways.
- **A separate transition list per track**, keyed by the two clips: needs repair on every edit
  that removes, splits or moves a clip; the clip-owned field moves with its clip for free.
- **Rejecting a transition without handles**: honest, but blocks ordinary edits later; the
  dormant cut keeps edits working and the UI reports the shortened or plain cut.

## Consequences

- The project format (ADR-0007) must store `transition_in` per clip.
- The UI shows the playable span, which can be shorter than the requested duration, and says
  when a transition stays a plain cut.
- Transitions on audio lanes are allowed by the model (crossfades); the UI offers them on the
  storyline only for now.
- Not decided here: transitions between clips on different tracks, custom curves, and
  transitions longer than either clip.
