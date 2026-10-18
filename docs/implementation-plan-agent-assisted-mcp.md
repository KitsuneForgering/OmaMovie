### Conditional pilot — Agent-assisted editing through MCP (after M7)

This is a research gate, not a committed v0.1 deliverable. Use the concrete
exact-time edit task and alternatives in [MCP research](Research/agentic-mcp.md)
to decide whether MCP earns a place in the product. Write an ADR before adding
AI, a dependency or a new process boundary (`CLAUDE.md` §§2, 20–21).

- [ ] After durable project save/load, expose a local, client-launched `stdio` prototype scoped to one chosen project. Pin an MCP protocol/SDK revision and test a real external host; do not add an HTTP listener or embedded model for this pilot.
- [ ] Expose bounded read-only project/media/timeline data with stable IDs, exact rational times, pagination and a persisted content revision. Optional thumbnails require explicit count/resolution and project access limits; media metadata is untrusted content.
- [ ] Add `propose_edit` for a small typed command set, dry-run validation on a copy and a structured before/after diff. Reject unknown commands, invalid ranges and stale revisions. Show the diff in OmaMovie; apply an accepted batch via the existing `Editor` as one undo transaction, with a fresh revision check. The ADR must settle GUI versus headless ownership and retry behavior before any mutating MCP tool is exposed.
- [ ] Compare the same synthetic exact-time tasks with GUI and deterministic `oma-project` CLI/macro baselines. Require correct final timeline, no unintended edit, one-step undo, stale-edit rejection and no action from adversarial media metadata. Record host/model/version, tool calls, corrections, elapsed time, privacy exposure and failures; only expand to representative user tasks if the pilot passes.
- [ ] Revisit background export/proxy jobs only after M7/M8 job APIs exist. Use the MCP Tasks extension if a chosen host supports it and the job needs durable polling/cancellation; short edit proposals do not need Tasks.

If CLI or GUI completes the task just as well with lower cost, stop at the
deterministic command surface. No agentic capability is validated by this plan.

[← Implementation plan](implementation-plan.md)
