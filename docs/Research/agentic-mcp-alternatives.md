## Alternatives and discriminating checks

| Option | Benefit | Cost / decisive check |
|---|---|---|
| Manual GUI baseline | Existing undo and visible timeline. | Measure correction steps, completion and unexpected changes for the same exact-time tasks. |
| Deterministic `oma-project` CLI or macro | Smallest reuse of existing commands; reproducible without a model. | If it handles the task with equal effort/accuracy, MCP adds little product value. |
| MCP read/propose adapter | External hosts can discover typed project data and actions. | Check real host compatibility, typed-output fidelity, privacy and maintenance cost; not proof of model quality. |
| Embedded AI assistant | Integrated preview/approval may fit editing better. | Requires a chosen model, UX, cost/privacy policy and MCP **client** only if connecting to external servers; defer until the external-host pilot identifies unmet needs. |

**Proposed local pilot, not executed:** after M7, use small synthetic projects
with stable media and an adversarial filename/metadata string containing tool
instructions. Run 10–20 exact-time editing tasks through GUI, CLI and one pinned
MCP host/model on the same fixtures. Record version, prompt, tool calls, elapsed
time, user corrections, final timeline diff, undo result, disclosure of media
content and errors. Require no unintended edits, correct atomic undo, stale
revision rejection and no action induced by the adversarial metadata before
considering broader use. Compare completion effort only on successful tasks;
report failures separately. Sample size is a pilot, not a general usability
estimate. A second pilot would need representative user tasks and media.

[Return to agentic-mcp.md](agentic-mcp.md)
