## Alternatives, tests, and stopping rules

| Choice | Simpler baseline / cost | Proposed discriminating check (not executed) |
|---|---|---|
| Anchored dependents | Keep track-local ripple and manually group clips. Anchors add edge cases and migration rules. | Construct a short edit with title, cutaway and detached voiceover. Move, split, ripple-trim and delete the primary clip; save/load and undo/redo each action. Accept only if relative timing follows the written policy, locked tracks behave explicitly, and no invalid/dangling anchor survives. In M6, compare mistakes and recovery against manual grouping. |
| Explicit position mode | Reuse existing overwrite/remove commands without a mode. A mode adds state and discoverability work. | At wide/half/narrow layouts, ask users to replace a shot without moving later edits and then restore it. Require the destination, displaced material, and gap to be predictable, including keyboard-only operation. |
| Prior project version | Autosave alone is simpler and covers interruption, but also saves user mistakes. Versions use storage and need retention/restore UI. | Inject interruption during save and reopen; separately make an unwanted edit, autosave, then restore an earlier valid version. Accept only if project integrity and media references are preserved; document that source files require separate backup. |
| Proxy representation | First try bounded decode-ahead, reduced preview quality, and background render. Proxy generation costs time and disk. | On representative 1080p/4K phone and recording fixtures, compare smoothness/tail frame time, seek delay, generation time, disk use and output fidelity at fixed hardware/driver. Accept only for workloads where the end-to-end benefit is material; export must resolve originals or fail clearly. |
| Roles/keywords | Filename/date search and existing tracks are cheaper. Metadata and stem export add persistence/UI work. | Defer until users cannot find source shots or repeatedly need separate audio deliverables in observed tasks. |
| FCPXML | OTIO first has a public adapter/schema path already planned. FCPXML adds versioned bundle, timing and feature-mapping work. | Import actual version-pinned exports covering cut, connected clip, gap, audio and transition. Compare duration, order, anchors and media references; report every unsupported object. No blanket “Final Cut compatible” label from parsing alone. |

This review executed **documentary checks only**: Apple guide passages and the
local model/plan were inspected. No Final Cut installation, exported fixture,
user study, workload benchmark, or crash injection was run. The acceptance
criteria above are proposed before results. No comparative performance or
usability claim follows from this review.

[Return to final-cut-pro.md](final-cut-pro.md)
