## 3. Milestones

Execution contract for this plan:

- `[x]` means the stated, bounded deliverable exists and has the cited check. A dated “Partial” note describes an earlier stage; any remaining work must have its own `[ ]` item.
- `[ ]` means work remains. A milestone closes only when its **Done when** checks pass for the declared platform and feature scope.
- A conditional pilot is a decision experiment, not a release gate. Record the result and either promote a bounded deliverable or close the proposal without implementing it.
- External hardware, third-party formats or an unavailable host cannot hold the Intel/software release path open. Record an unvalidated path and exclude its support claim until hardware and checks exist.
- A failing check requires a fix or a narrower honest support claim. Never mark it complete because a workaround was proposed.
- File size is an inventory signal, not a release or architecture gate. Partition a file when mixed ownership, change coupling or test friction is demonstrated; verify behavior after each extraction. The earlier blanket 60-line target is retired by the 2026-10-12 audit.
- A significant feature is done when implementation, error handling, behavior tests, regression coverage for discovered bugs, hot-path benchmark when relevant, architecture documentation/ADR when changed, and applicable CI gates pass. UI changes check both model and visible state when they can diverge.
- Do not add a database, index, cache, worker, backend, codec or command layer without the problem, evidence, simpler alternative and repeatable acceptance check.
- No release, deployment or public support claim follows automatically from checked boxes; the maintainer authorizes publication.

[← Implementation plan](implementation-plan.md)
