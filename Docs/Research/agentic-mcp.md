# Agent-assisted editing with MCP

> Skeptical research, 2026-10-04. This is an architecture and product pilot,
> not an implemented feature or a claim that an LLM can edit video reliably.

## Conclusion and scope

An OmaMovie MCP **server** is a plausible adapter for an external AI host to
inspect a local project and propose edits. It should call the same validated
timeline/project operations as the UI, and keep the model outside the rendering
engine. The first useful task is bounded: “In this saved project, split the
selected shot at a specified time, remove a specified range, and lower a named
audio clip's gain; show the exact proposed changes before applying.” This tests
command composition and human review without assuming speech recognition,
semantic scene understanding, or automatic selection of “good” footage.

The pilot belongs **after M7 save/load**. Today the README says sessions cannot
be saved or exported. The existing [Editor API](../../libs/timeline/include/oma/timeline/editor.hpp)
already supplies atomic commands, undo/redo and a changing in-memory revision;
its state belongs to the UI thread. That is a credible execution core, but not
yet a durable cross-process automation surface. An ADR is required before
committing to AI or a new dependency under `CLAUDE.md` §§2, 20–21.

## Question, hypotheses and decision criteria

**Question:** Does a local MCP adapter help a user complete precise, reviewable
edits in OmaMovie better than the current GUI or a deterministic CLI/script?
Criteria set before results: correct media/time selection, no hidden edits,
recoverable one-step undo, refusal of stale/conflicting proposals, bounded
media exposure, and task completion effort. Cost includes implementation,
model/API use, data disclosure and extra approvals. The first research budget
was four targeted search passes plus the official protocol's architecture,
tools, resources, transport, security and Tasks pages, and local API inspection.

| Hypothesis | Prediction that would support it | What would weaken it |
|---|---|---|
| H1: structured project tools improve edit planning | In the same exact-time tasks, an external host using OmaMovie tools makes fewer invalid operations or needs less user correction than generic file/shell access. | No advantage over a small CLI or a saved edit macro. |
| H2: proposal + review makes agent edits controllable | Users can identify the affected clips/ranges before apply, reject wrong edits, and undo an applied batch in one step. | The preview hides material side effects or users still accept wrong edits. |
| H3: MCP is the right interface | A compatible host can discover, read and call the tools without OmaMovie-specific integration while the core logic remains reusable. | Host/version support or protocol maintenance costs outweigh reuse; a CLI meets the use case. |

“Agentic” here means a host can choose a sequence of project queries and edits
to meet a user goal. MCP supplies the tool contract; it does not provide the
model, visual understanding, reliable planning, or safe authorization. The
[MCP architecture overview](https://modelcontextprotocol.io/docs/2026-07-28/learn/architecture)
separates host, client and server and says the protocol does not dictate how an
AI application uses an LLM. This is a **verified protocol distinction**;
OmaMovie's proposed workflow is an **engineering inference**.

## Protocol evidence and design consequences

| Official contract, 2026-07-28 | Consequence for OmaMovie |
|---|---|
| [Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools), “Tool”, “Structured Content”, “Security Considerations”: tools have JSON Schema input and optional output, structured results, and server-side validation/access control. Tool annotations are untrusted hints. | Publish a small set of typed read/propose actions. Validate stable IDs, rational times, ranges and project access in the server. Do not use annotations or prompt text as enforcement. |
| [Resources](https://modelcontextprotocol.io/specification/2026-07-28/server/resources), “Resource Contents” and “Security Considerations”: text/binary resources, templates and URI/path validation. | Expose bounded project summary/timeline/media metadata and optional small thumbnails, not arbitrary paths or raw project files by default. Media filenames and metadata returned to a model are untrusted data. |
| [stdio transport](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio) and [security guidance](https://modelcontextprotocol.io/docs/2026-07-28/tutorials/security/security_best_practices), “Local MCP Server Compromise”: local child-process transport avoids a listening port; a local server still has the user's filesystem privileges. | Pilot with a packaged, client-launched `stdio` server, scoped to one explicitly chosen project. No network listener, arbitrary shell tool or unrestricted filesystem resource. Restrict process permissions where feasible. |
| [2026 revision overview](https://blog.modelcontextprotocol.io/posts/2026-07-28/), “No handshake or sessions” and “Tasks”: cross-call state uses explicit handles; older lifecycle assumptions changed. | Pin a protocol/SDK revision and test real hosts. A project handle is not authorization; use project identity plus a persisted content revision and check both again at apply time. |
| [Tasks extension](https://tasks.extensions.modelcontextprotocol.io/), “Architecture” and “Cancellation”: long work can return a durable task ID; cancellation is cooperative and requires client support. | Export/proxy work may use Tasks later, only after M7 jobs exist and the chosen host supports the extension. A short edit proposal does not need a task store. |

The 2026-07-28 spec is the inspected version, not a promise of support by every
host. Annotations such as `readOnlyHint` help clients present risk but are
explicitly not security controls; see the [maintainers' explanation](https://blog.modelcontextprotocol.io/posts/2026-03-16-tool-annotations/),
“What Annotations Can't Do”. MCP security guidance also warns that explicit
state handles must not be treated as authentication (“State Handle Hijacking”).

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

## Source and stopping record

Accessed **2026-10-04**. MCP project/maintainer pages above were read in HTML;
their protocol version is 2026-07-28, while individual page publication/update
dates were not exposed except the dated 2026-07-28 release post and 2026-03-16
annotation post. These pages share the MCP project's evidential lineage and
establish protocol behavior, not editing effectiveness or actual host support.
Local `README.md`, `CLAUDE.md`, `Docs/implementation-plan.md`,
`libs/timeline/include/oma/timeline/editor.hpp` and `edit.hpp` were inspected
from this working tree, which contains uncommitted changes. No MCP server,
host integration, security test, user study or benchmark was executed. The
investigation stops at a bounded design: the decisive missing observation is
whether a real host improves precise edits over CLI/GUI once durable projects
exist. If not, keep the deterministic automation surface and stop the MCP pilot.
