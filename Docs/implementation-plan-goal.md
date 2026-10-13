## 1. Goal

Build a native local editor for simple recording/phone-video workflows on Omarchy, with
an extensible pipeline. Offline/no-account behavior, a contextual UI and GPU-first processing
are project choices. Real-time playback and support breadth are claims earned per tested
workload, hardware and driver. Professional parity and replacing the default editor are
long-term aspirations, not findings established by this research.

Research basis after the [skeptical review](Research/skeptical-review.md):

| Premise | Evidence / status | Effect on the plan |
|---|---|---|
| Consumer-media accessibility matters | Bounded Resolve 20 Linux codec rows; audience hypothesis | Keep phone/recording fixtures; compare current Kdenlive/Shotcut baselines |
| Recording integration | Installed scripts, version-sensitive directory/codec/audio behavior | Respect directory overrides and no-audio files; probe actual output |
| Recoverable edits and projects | Explicit lifetime/undo and Linux durability contracts | Atomic replacement plus file/directory sync, autosave and parser limits |
| Long-form editing | Duration, clip count, codec/GOP, effects and export stress different paths; current source inspection, no long-form benchmark | Preserve JSON/simdjson pending measured need; use the workload gate in [long-form research](Research/long-form-editing.md) before indexes, proxies or a new project store |
| Contextual/magnetic/preset UI | Documented feature examples; effectiveness untested | Preserve design choices; M6 baseline/task pilot before claiming usability |
| Final Cut connected editing | Apple documents primary/connected clips and a precise Position tool; OmaMovie ripple is track-local | Specify anchors and a non-ripple placement path before freezing the native project DTO; test against manual grouping ([research](Research/final-cut-pro.md)) |
| VEGAS Pro 23 editing | Official help documents source in/out selection, explicit ripple scopes, one-way sync links and alternate takes; no OmaMovie task study | Pilot source selection and ripple preview in M6; compare takes with overwrite after M7; keep current anchors/transitions unless a task shows a gap ([research](Research/vegas-pro.md)) |
| Agent-assisted editing with MCP | MCP defines external-host tools/resources, not a model or editing quality; OmaMovie has validated commands and durable project save/load, but no persisted content revision | Keep a conditional local read/propose pilot after M7, compare against GUI/CLI, and require an ADR before implementation ([research](Research/agentic-mcp.md)) |
| Shared Vulkan device | ADR-0004 headless queues; S4/ADR-0005 validated Qt zero-flag queues for a static preview | M4 must pause live producers across Qt swapchain changes |
| Color | Current SDR subset; independent oracle and display stages pending | S6/ADR-0006; no HDR claim from RGBA16F storage |
| NVIDIA | Packaged FFmpeg runtime candidate; no hardware validation | S7; no universal speed ranking or toolkit-free build claim |
| OpenCL excluded | Existing maintainer scope decision; generic interop exists | Keep exclusion; no unsupported impossibility rationale |

[← Implementation plan](implementation-plan.md)
