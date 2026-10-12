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

[Return to agentic-mcp.md](agentic-mcp.md)
