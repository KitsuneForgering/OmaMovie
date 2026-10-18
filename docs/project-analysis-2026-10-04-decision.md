## Decision

Finish and verify the **import → edit → save → reopen → export** path before adding more
effects, interchange formats or extension mechanisms. The existing time model, command
based editing and GPU compositor are useful foundations. The main product gap is that a
user still cannot deliver a movie from the app. The next performance work should measure
the complete viewer and large timeline interactions; the current spikes do not establish
either claim.

This is a source and document review of commit `1654e1b` plus an uncommitted working tree
on 2026-10-04. Several implementation and evidence files were already modified before
this review; their changes were not audited as a final revision. Inspected paths:
`apps/omamovie` session, viewer, scheduler and QML; `libs/timeline`, `libs/playback`,
`libs/project`; the implementation plan and recent research/evidence. I did not rerun
tests, GUI playback, benchmarks or failure injection. The 2026-10-04 `make test` result
and S5 timings cited below are [existing records](Research/evidence/2026-10-04-verification.md),
not new observations in this review.

[Return to project-analysis-2026-10-04.md](project-analysis-2026-10-04.md)
