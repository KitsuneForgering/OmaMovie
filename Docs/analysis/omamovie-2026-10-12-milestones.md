## Milestone reconciliation at `635ffad`

This is an audit of stated scope, not a mass recheck of every historical benchmark.
Keep milestone numbering and the order of the release path. Checked historical
deliverables stay checked when the bounded implementation and cited evidence exist;
an open support path remains open rather than silently broadening a claim.

| Milestone | Current evidence and disposition |
|---|---|
| M0 Foundation | Closed for build/library baseline; PR workflow exists, but merge enforcement is now an M10 gap. |
| M1 Hardware spikes | S1–S6 recorded on Intel; complete for that scope. S7/S8 wait for NVIDIA/AMD machines. |
| M2 GPU/media | Intel DMA-BUF/Vulkan path and fallback exist; cross-device/hybrid claims remain unvalidated. No driver-internal copy claim. |
| M3 Compositor | SDR CPU/Vulkan and vectors exist; HDR and some format/device coverage remain open. Keep multi-pass backend conditional. |
| M4 Playback/audio | Intel M4 closed for stated workload; retimed sound policy is M8, expanded races/long-form are M10. |
| M5 Timeline model | Closed for core typed commands/time maps/anchors. Remove stale “not yet” items for captions, keys, transitions and Qt model. |
| M6 UI | Substantial source, precise edits and recordings exist; visible complete task, flaky menu route and responsive check remain open. |
| M7 Project/export | Versioned format, recovery, four profiles and export exist; write-boundary/content gate and installed package remain open. |
| M8 Creator | Captions, titles, transitions, some keyframes and speed presets shipped; retimed audio and broader automation remain open or conditional. |
| M9 Interop/color | OTIO/FCPXML and HDR/scopes are planned; no v0.1 parity claim. Retain receiving-task pilots. |
| M10 Validation | Parser fuzz and CI matrix exist; branch ruleset, app sanitizers, stateful properties, QML workflow and full release task open. |
| M11 Refactoring/UI | Planned after behavior baseline; replace blanket line-count gate with focused ownership and responsive workflow checks. |

Do not mark M6/M7/M10 closed from headless model tests or output-file existence.
Likewise, later M8 features can be implemented ahead of order without closing the
earlier end-to-end gates. Obsolete statements were corrected in M5, the FilmCraft
breadth note, the long-form premise and M11; historical measurements retain dates.

[← Assessment](omamovie-2026-10-12.md)
