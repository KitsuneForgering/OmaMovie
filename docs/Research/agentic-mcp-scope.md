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

[Return to agentic-mcp.md](agentic-mcp.md)
