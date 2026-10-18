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

[Return to agentic-mcp.md](agentic-mcp.md)
