## Proposed architecture and minimal contract

```text
external AI host + MCP client
            | local stdio (one chosen project)
       oma-mcp adapter
            | typed, bounded calls
 project service / Editor commands / M7 persistence
            | user-visible proposed diff and one undo transaction
       OmaMovie UI
```

The adapter is **not** a plugin loader or a second timeline implementation.
For an offline project, it can reuse a headless application service after M7;
for a project open in the GUI, the bridge must marshal edits to the owning UI
thread. Do not let two independent `Editor` instances overwrite the same file.
Before implementation, an ADR must choose one ownership path and specify
locking, save, crash recovery and UI presentation.

Suggested initial operations, subject to the ADR and a real host test:

1. Read-only `project_summary`, `list_media`, `read_timeline` with pagination,
   stable clip/media IDs, rational time as numerator/denominator/timebase, and
   a persisted project content revision. Optional thumbnail sampling is
   explicitly bounded by count, resolution and media path; no transcript or
   scene semantics are claimed.
2. `propose_edit` accepts a bounded list of **typed existing commands** and an
   expected project revision; it dry-runs validation on a copy, returning a
   structured before/after diff, warnings, affected clip IDs and a proposal
   token. The server rejects free-form code, shell commands and unknown fields.
3. The OmaMovie UI shows the proposal and lets the user apply or reject it.
   Apply checks project identity and revision again and executes one
   transaction through `Editor::execute`; stale proposals fail without change.
   If a mutating MCP tool is later needed, require an approval artifact minted
   by the trusted app after showing the exact diff. A model-supplied boolean is
   not approval. Design retry/idempotency behavior before exposing that tool.

Read-only tools can be useful sooner, but cannot validate “agent edits” alone.
The pilot does not require an embedded chat panel, a specific model provider,
network access, or the MCP Tasks extension. A fixed tool vocabulary also avoids
making every low-level timeline primitive an independent model action.

[Return to agentic-mcp.md](agentic-mcp.md)
