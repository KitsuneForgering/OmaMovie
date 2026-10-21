# Implementation plan — OmaMovie

Evidence, milestones and acceptance criteria; update completed work and record structural changes in an ADR. [Current engineering assessment](analysis/omamovie-2026-10-12.md). Latest: [evidence status 2026-10-14](implementation-plan-evidence-2026-10-14.md) · [async interactive work](implementation-plan-async-interactive.md). `Implemented` means wired into code; `tested` has a relevant test; `measured` has a repeatable benchmark; `validated` has a declared workload and recorded result; `supported` is a bounded public commitment backed by appropriate evidence; `planned` is none of these.

### Current 4K interpretation (Intel reference hardware)

The Intel Core i5-1135G7 / Iris Xe is already capable of useful 4K editing workloads. This is a bounded finding, not a general 4K60 professional-workflow claim. [S3](spikes/S3-decode-paths.md) measured strong hardware decode throughput on its codec fixtures; the viewer's VA-API → Vulkan import avoids mandatory CPU pixel readback on the [validated Intel path](implementation-plan-m4-playback-audio.md). A bare 4K compositor pass measured 8.2–8.9 ms ([effect pass cost](Research/evidence/2026-10-14-m10-effect-pass-cost.txt)); a 4K hardware-viewer fixture reported about 11 ms composition and several 4K playback fixtures passed their recorded app audits ([FilmCraft-equivalent measurements](analysis/omamovie-2026-10-14-filmcraft-benchmarks.md)). [Paused 4K scrub](implementation-plan-async-interactive.md) passed its UI gap gates after async decode, and the basic adaptive preview mechanism is implemented in [M4](implementation-plan-m4-playback-audio.md). A single 4K23.976, 240-frame export ran near the fixture rate with VA-API H.264 encoding despite measured readback/copy/upload ([phase split](Research/evidence/2026-10-16-export-transfer-phases.md)); this is a diagnostic run, not a supported export-speed promise.

The remaining ceiling depends on source codec/GOP, simultaneous streams, effects, scheduling, caches, preview policy and transfer boundaries. The [three-source spike](spikes/professional-workload.md) reports *sequential decode + render deadline overruns*, not actual player dropped frames, and does not validate 4K60 playback. FilmCraft's public measurements suggest GPU rendering helps effects-heavy scenes, while plain clips can remain decode/encode-bound; its Linux VA-API figures include costly `vaGetImage` readback. Its proxy, GOP-cache, draft-decode, disposable-picture and memory-budget policies are useful hypotheses. Its custom codecs are not a reason to replace OmaMovie's FFmpeg foundation. The published figures use other machines and methods, so [the comparison](analysis/omamovie-2026-10-14-filmcraft-benchmarks.md) is not a speed ranking.

| Mechanism | Main cost addressed | Status / boundary |
|---|---|---|
| Proxy / optimized media | Source decode, seek and bandwidth | Planned conditional [M8 pilot](implementation-plan-m8-creator.md); original media remains authoritative for final export |
| Adaptive preview | Real-time viewer resolution and render cost | Implemented and tested on Intel in [M4](implementation-plan-m4-playback-audio.md); heuristics, effect awareness and broader hardware validation remain future work |
| Decoded/GOP cache | Re-decode and seek/reverse | Bounded frame history is implemented in [M4](implementation-plan-m4-playback-audio.md); broader cache policy is future work |
| Render cache | Repeated effects/composition work | Planned conditional [M8 pilot](implementation-plan-m8-creator.md) |
| GPU-native encoder input | Export readback, host copies and upload | Planned and measured as a transfer problem in [M12](implementation-plan-m12-gpu-export-architecture.md) |

These mechanisms solve different costs; proxies are not a prerequisite for the bounded 4K workloads already measured. A proxy must leave timeline semantics unchanged: original media → proxy for interactive editing; original media → final render/export. The v0.1 decision remains limited to honestly validated Intel/software workflows. Full proxy workflow, render cache, GPU-native export, end-to-end video zero-copy, AMD/NVIDIA, multicam, HDR and 6K/8K optimization are not v0.1 gates.

### Post-v0.1 4K headroom validation (planned, outside the release gate)

Keep fixtures, scene, host, driver, power/load state, build, cache state and path logs reproducible; repeat runs before making a performance comparison. Measure decode and compositor p50/p95/p99, UI gap, *actual player* drops, process CPU/frame, memory and negotiated path where applicable. Extend [S3](spikes/S3-decode-paths.md), [S5](spikes/S5-minimal-compositing.md), the [professional workload spike](spikes/professional-workload.md) and [M10 pass-cost evidence](Research/evidence/2026-10-14-m10-effect-pass-cost.txt) in this order:

1. Single stream: 4K H.264, HEVC and HEVC Main10; software versus hardware decode; full versus adaptive preview.
2. Multiple streams: 2×4K then 3×4K, at 4K24 and 4K60 where meaningful; simple transforms before effects. Report sequential spike overruns separately from player drops.
3. Effects: no effect → fused color/look → blur → sharpen → multiple affected layers; measure marginal time and memory.
4. Proxy: rerun the hardest *same* scenes to isolate source decode/seek cost from composition/effects.
5. Render cache: rerun those effect-heavy scenes to measure eliminated repeated rendering.
6. After [M12](implementation-plan-m12-gpu-export-architecture.md), rerun the exact 1080p60 and 4K23.976 export fixtures to quantify readback/copy/upload cost. Do not assign a speedup before a controlled A/B.

### Architecture and release discipline

Preserve synchronous, deterministic `timeline::evaluate` and commands apart from the compositor; move only execution across async boundaries ([ADR-0020](adr/0020-async-execution-boundary.md)). Retain CPU reference paths, explicit GPU ownership, FFmpeg as the media foundation and the existing QML-facing `Session` facade. Recent gains came from scheduling, cancellation, bounded queues/cache, ownership and lifetime fixes, pass fusion and explicit synchronization. Make measured vertical changes; extract `Session` state only when an independent lifecycle has a real owner ([M11](implementation-plan-m11-refactoring.md), [M12](implementation-plan-m12-gpu-export-architecture.md)). Do not introduce a universal GPU/compute backend, service locator, global event bus, dependency-injection framework, controller hierarchy or custom codec stack on speculation.

**Source-path review (2026-10-19, `ad276a9`).** A repository inventory and focused trace of
editing, playback, export, project persistence, recovery and QML found no basis for a
whole-engine rewrite. The sorted-clip lookup, visible-range storyline model, bounded
playback/encode queues and durable project-save path are useful existing choices. The
measured export transfer problem and the intermittent long-timeline split sample are
already assigned to [M12](implementation-plan-m12-gpu-export-architecture.md) and
[M6/M10](implementation-plan-next-step.md). One additional *derived scaling risk* was
then measured and closed without a code change (see below): `timeline::evaluate` asks `clip_at(track.id)` for each track, which searches
the track vector again before binary-searching its clips; `Timeline::find_media` then
scans the media vector for each active clip, and `build_viewer_frame` can repeat that
media lookup. With `T` tracks, at most `C` clips per track, `A` active clips and `M`
media entries, the non-decoding work is approximately `O(T² + T log(C+1) + A·M)` per
frame, before effects and pixel work. The [lookup scaling benchmark](Research/evidence/2026-10-19-evaluate-lookup-scaling.md)
measured at most 94 µs per frame at 64 tracks and 2,000 media entries (under 1% of a
60 fps frame) and about 12 µs at 16 tracks and 500 media, so no index is added. Revisit
only if a real playback/export profile attributes measurable time to `evaluate`; then
search clips on the already available `Track` and remove the duplicate media query
before comparing an index on the immutable export snapshot.
Export power-loss durability remains outside the v0.1 claim as [M7](implementation-plan-m7-project-export.md)
already states; process-crash recovery tests do not establish it.

Significant releases require a release candidate, especially after project-format, timeline, playback, GPU, export, recovery, proxy, cache or hardware-interop changes: development → feature freeze → RC → real-world validation → blocker/regression/compatibility fixes only → another RC if needed → stable. Avoid new features, broad refactors and opportunistic cleanup during RC. Repeated fixes that break previously fixed behavior are evidence to inspect a boundary; an isolated fix with a regression test usually points to a local defect. The [2026-10-17 correctness pilot](Research/evidence/2026-10-17-rc-correctness-pilot.md) was one dirty-checkout round, not a frozen-SHA RC pass; follow the [RC gate](implementation-plan-cross-editor-benchmarks.md). Ship early within precisely described support.

### Context-menu-first, with contextual surfaces (UX direction; planned)

The default editing view remains the library, viewer and timeline. The selected object determines which actions apply; its context menu is an entry point both for immediate commands and for richer, temporary task surfaces: `Split` performs an edit, while `Color…` opens the controls for a color task. **Interface complexity should follow the complexity of the current task, not the sum of the editor's capabilities.** A capability existing does not mean its control must stay permanently visible. Preserve the existing context-menu-first approach, keyboard access when convenient, the command palette, accessibility and on-demand properties. The aim is less permanent interface for the same capability, not fewer features or a ban on cockpit-like controls when a task needs them.

| Task size | Appropriate surface | Examples |
|---|---|---|
| Small | Inline control, menu or popover | Rename, enable/disable, simple gain, small parameters, Split and Delete |
| Intermediate | Temporary contextual drawer or side sheet | Speed, transform, crop, clip properties and other compact parameter sets |
| Specialized domain | Temporary full contextual surface, with cockpit-like controls when justified | Future color grading, audio mixing, advanced effects/VFX, multicam, captions, advanced delivery/export and scopes or other technical tools |

This extends the existing [M6 UI](implementation-plan-m6-ui.md): the clip menu already opens Color, Effects, Crop, Volume and Info drawers, with advanced grading, EQ and noise controls available on demand. These are implemented drawers, not full specialized screens. Keep them where they fit; promote a workflow to a fuller surface only when its real controls and task evidence justify the space. Do not start from permanent Editing/Color/Audio/Graphics workspaces or separate simple/pro modes. A representative future path is `selected clip → context menu → Color… → contextual color surface for that clip → return to editing`. The originating clip, cut, track or project must be carried into the operation, so opening a surface does not make the user find and select the target again. Define what happens if that target is deleted or becomes inapplicable while the surface is open.

The existing `ActionRegistry` attaches the `OmaAction`s declared in `Main.qml`, gives them stable property-name IDs, manages remappable shortcuts, searches them for `Ctrl+K`, and triggers an enabled action by ID. The current Color and Effects actions already open drawers from both menu and palette; immediate editing actions also use this route where applicable. Extend those actions and handlers as needed so menu, shortcut, palette and any future `:color`-style command invoke the **same action and target resolution**. An action may perform an edit or express intent to open a contextual surface; the navigation intent belongs in its handler, while edit behavior remains in the existing `Session`/timeline commands. Do not add a generic action framework or duplicate handlers by input device. Some existing menu items still call `Session` directly; convergence for them is future work, not a claim about the current implementation. Parameterized commands need their own explicit target/argument decision when implemented.

Navigation should be shallow and reversible: `Timeline → Color → Curves`, then `Esc`/Back returns `Curves → Color → Timeline`, restoring a usable focus position and editing context. This describes the interaction, not a requirement to implement `SurfaceStack` now. Avoid deep chains of screens; prefer a control within the current surface when another navigation level would make the user lose the task. Opening and leaving a surface must preserve the project and selection semantics, provide keyboard and screen-reader routes, and remain usable at the tiled/narrow widths covered by the M6 checks.

**Decision gate for future UI work:** first prototype one real intermediate or specialized workflow using the existing actions and drawers where possible. Compare discoverability, task completion, focus/Back behavior and narrow-window usability against the current drawer path before extracting a shared navigation mechanism. Record any structural change in an ADR. This direction does not change the v0.1 release gates or mark future domain surfaces as implemented.

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
