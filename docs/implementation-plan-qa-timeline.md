## QA — timeline properties and exact replay

Build a small generator around `Timeline`, `Editor` and existing `edit::*` commands.
Generate valid media, compatible tracks, clips, anchors and exact sequence-grid times;
derive legal command preconditions from the generated state. Begin with append,
insert, split, trim, ripple delete and undo/redo; expand by failure value to
overwrite, remove, ripple trim, roll, slip, slide, cross-track move, reverse,
freeze frame, speed changes and segmented time maps. Include deliberately rejected
commands and boundary values. Keep the existing M5 example-based tests.

After every successful edit, check `Timeline::validate()` and independent invariants:
no overlap within a track; valid nonzero durations; media bounds; unique IDs;
clip/track kind compatibility; source continuity where the command promises it;
edit points on the sequence grid; checked arithmetic with no silent overflow; valid
time-map coverage and media extent; and successful native serialization/reload for
serializable states. Check the observable timeline, revision and history position
after rejection. A rejected edit must leave undo and redo behavior unchanged, not
merely leave `can_undo()` true. Check anchors and connected clips as they enter the
generator. Avoid using `validate()` alone as an independent oracle.

Use a **test-only canonical state** containing ordered tracks, clips and IDs, source
ranges, time maps, anchors, effects, transitions, markers and other persisted edit
fields. Omit transient UI data and serialization formatting. The existing project
format can verify round trips but should not define the sole comparison oracle.
Compare history behavior with saved states and next undo/redo operations; add a
test-only history snapshot only if public observations cannot distinguish a bug.
Allocate IDs through `Editor`; check uniqueness and monotonic non-reuse separately,
since undo restores a timeline snapshot but does not roll back the ID allocator.

For each generated valid command sequence, save canonical states S0…Sn. Undo once
per successful command and compare to S(n-1)…S0, then redo and compare to S1…Sn.
Check the initial and final states exactly, including source mapping and anchors.
`revision` should advance on successful execute/undo/redo, not rewind to an earlier
value. A rejected command must leave it unchanged. Do not treat history flags alone
as proof of reversibility.

Every run accepts an explicit seed and prints the actual seed, generator version,
initial timeline fixture and a serialized, ordered command log with IDs and exact
rational values. Replay must need no random choices after reading that log. Shrink
by removing command spans, simplifying media/track topology and reducing numeric
values while preserving a valid prefix and the same failure. Print a direct replay
command and minimized case; keep failing seeds as temporary triage material, then
commit a small deterministic regression. If shrinking cannot preserve the failure,
retain the full serialized case. No unreproducible property test is accepted.

Keep fast, fixed-seed cases in `make test` and CI; run larger seed budgets separately.
Count useful state transitions and distinct command interactions rather than only
test count. A new abstraction is justified only where the current `tests/timeline`
fixtures and `layout()` omit semantic state: the minimal additions are a canonical
test printer, invariant helpers and a command-log replay driver, all outside the
production editing API.

[← QA strategy](implementation-plan-qa.md)
