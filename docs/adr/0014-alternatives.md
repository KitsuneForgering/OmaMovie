## Alternatives considered

- **Offset from the primary's start in sequence ticks.** Simpler, but a start trim moves the
  dependent off the content it belonged to; per-edit compensation would recreate the
  content model with more special cases.
- **Manual grouping (no relation).** No persisted relation means undo, save/load and edits
  outside the UI break it; kept as the comparison baseline for the M6 task study.
- **Nested anchors and anchoring to segmented primaries.** Need an inverse of non-monotonic
  maps and transitive moves; deferred until a task needs them.

[Return to the decision](0014-connected-clips.md)
