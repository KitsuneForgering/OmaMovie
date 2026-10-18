# Implementation plan — OmaMovie

Evidence, milestones and acceptance criteria; update completed work and record structural changes in an ADR. [Current engineering assessment](analysis/omamovie-2026-10-12.md). Latest: [evidence status 2026-10-14](implementation-plan-evidence-2026-10-14.md) · [async interactive work](implementation-plan-async-interactive.md). `Implemented` means wired into code; `tested` has a relevant test; `measured` has a repeatable benchmark; `validated` has a declared workload and recorded result; `supported` is a bounded public commitment backed by appropriate evidence; `planned` is none of these.
## 1. Goal
[Goal](implementation-plan-goal.md)
### Cross-editor workflow gap review (2026-10-11)
[Cross editor workflow](implementation-plan-cross-editor-workflow.md)
### FilmCraft breadth check (2026-10-11)
[Filmcraft breadth](implementation-plan-filmcraft-breadth.md)
## 2. Milestone overview
[Milestone overview](implementation-plan-milestone-overview.md)
## 3. Milestones
[Milestones](implementation-plan-milestones.md)
### M0 — Foundation ✅ (2026-10-02)
[M0 foundation](implementation-plan-m0-foundation.md) <a id="m0--foundation-"></a>
### M1 — Hardware spikes
[M1 hardware spikes](implementation-plan-m1-hardware-spikes.md)
### M2 — `libs/gpu` and `libs/media`
[M2 gpu media](implementation-plan-m2-gpu-media.md)
### M3 — Compositor and render graph
[M3 compositor](implementation-plan-m3-compositor.md)
### M4 — Playback and audio
[M4 playback audio](implementation-plan-m4-playback-audio.md)
### M5 — Timeline model (in parallel with M2–M4) ✅ (2026-10-05, model; UI in M6/M8)
[M5 timeline](implementation-plan-m5-timeline.md)
### Audio editing ahead of order (2026-10-03)
[Audio editing](implementation-plan-audio-editing.md)
### Video effects ahead of order (2026-10-03)
[Video effects](implementation-plan-video-effects.md)
### Modular effects architecture (planned; establish before expanding the catalog)
[Modular effects](implementation-plan-modular-effects.md)
### M6 — UI
[M6 ui](implementation-plan-m6-ui.md)
### M7 — Project and export
[M7 project export](implementation-plan-m7-project-export.md) <a id="m7--project--export"></a>
### Code cleanup during M6–M7
[Code cleanup](implementation-plan-code-cleanup.md) · [item-by-item code review](implementation-plan-code-review.md)
### Targeted RAII and lifetime work
[RAII and lifetime plan](implementation-plan-code-cleanup.md#targeted-raii-and-lifetime-work)
### M8 — Creator
[M8 creator](implementation-plan-m8-creator.md)
### M9 — Interop and color
[M9 interop color](implementation-plan-m9-interop-color.md)
### M10 — System validation and release gates
[M10 validation](implementation-plan-m10-validation.md)
### M11 — Backend/frontend refactoring and responsive UI
[M11 refactoring](implementation-plan-m11-refactoring.md)
### M12 — GPU-native export and application ownership (post-v0.1)
[M12 export and ownership](implementation-plan-m12-gpu-export-architecture.md)
### First-opening tour (v0.1) and contextual tooltips (v1.0)
[Onboarding and contextual tooltips](implementation-plan-v1-onboarding.md)
### Conditional pilot — Community effect packs and curated marketplace (after M7/M8)
[Community effect packs](implementation-plan-community-effect-packs.md)
### Conditional pilot — Agent-assisted editing through MCP (after M7)
[Agent assisted mcp](implementation-plan-agent-assisted-mcp.md)
## 4. Decisions and ADRs
[Decisions adrs](implementation-plan-decisions-adrs.md)
## 5. Tests and quality per milestone
[Tests quality](implementation-plan-tests-quality.md) · [QA strategy](implementation-plan-qa.md)
### Cross-editor benchmark and workflow comparison
[Comparison protocol](implementation-plan-cross-editor-benchmarks.md)
### Professional workload spike (experimental; outside the v0.1 gate)
[Three-source 4K60/8K60 diagnostic](spikes/professional-workload.md)
### Session stress testing
[Repeatable seeded stress runner](spikes/session-stress.md)
## 6. Hardware matrix
[Hardware matrix](implementation-plan-hardware-matrix.md)
## 7. Risks
[Risks](implementation-plan-risks.md)
## 8. Next step
[Next step](implementation-plan-next-step.md)
