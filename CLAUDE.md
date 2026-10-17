# OmaMovie — agent guide

OmaMovie is a native, local-first video editor for Linux and Omarchy, written mainly in
C++. It serves recordings, personal video and content creation through a simple,
contextual interface backed by an engine that can grow toward demanding workflows.
The useful reference is iMovie's approachability with some of Final Cut Pro's depth,
without copying either interface or promising feature parity.

## Source of truth and context loading

For mutable facts, use this order: **code and relevant tests → accepted ADRs → dated
benchmark/workload evidence → implementation plan → specialized Docs → this guide**.
An ADR records a decision, not proof that it shipped. Research is provisional. Resolve
conflicts by checking current code, tests and dated evidence, then correct the stale doc.

Load context progressively: this guide → one relevant skill if useful → the relevant
[implementation milestone](Docs/implementation-plan.md) and [next step](Docs/implementation-plan-next-step.md)
→ its [ADR](Docs/adr/README.md) → affected source/tests → evidence. Expand only when the
question requires it. Use [UI design](Docs/ui-design.md) for UX,
[format architecture](Docs/format-architecture.md) and [interop matrix](Docs/interop.md)
for formats, and [QA](Docs/implementation-plan-qa.md) for validation. [CONTRIBUTING.md](CONTRIBUTING.md)
contains build commands and contribution process; `make help` lists current targets.

## Facts, goals, preferences and evidence

**Facts** are checked in code: the app uses C++23, Qt Quick/QML, FFmpeg, Vulkan and
PipeWire; inspect current implementations before describing a path. **Goals** include
GPU-native export, clearer Session ownership and more explicit QML composition; the
[M12 plan](Docs/implementation-plan-m12-gpu-export-architecture.md) marks these as planned.
**Maintainer preferences** below guide choices but may be challenged by code or evidence.
**Evidence** lives in `Docs/Research/evidence/`, tests and workload protocols, not here.

Use status terms precisely: *implemented* = connected code; *tested* = relevant test;
*measured* = repeatable benchmark; *validated* = declared workload plus recorded result;
*supported* = bounded commitment backed by suitable evidence; *planned* = intended work.
Do not infer zero-copy, real-time performance, HDR, vendor or format support from a type,
architecture sketch or passing build.

## How to work

- Keep native-first, local-first and Linux/Omarchy-first behavior. Performance and
  accessibility are product requirements. Prefer a GPU path where justified, with a
  usable software fallback where feasible and an observable reason for downgrade.
- Measure before optimizing: baseline → hypothesis → instrumentation → change → same-fixture
  measurement → regression check. Inspect algorithmic cost as clips, frames, media and
  effects grow; hardware does not excuse a poor algorithm.
- Preserve behavior before restructuring. Extract by owned state, lifecycle, concurrency
  boundary or testable contract, not file length. Work in small vertical changes; reject
  decorative interfaces, global event buses, service locators and framework imitations.
- Use C++, Qt/QML, Vulkan, FFmpeg and PipeWire's own abstractions before inventing a new
  layer. Respect the dependency graph enforced by `cmake/OmaDependencyGraph.cmake` and
  `scripts/check-includes.sh`; inspect ownership, threading, time and CPU/GPU transfers
  before changing those boundaries. Record changed decisions in a new ADR.
- Let the engine own project and edit semantics. Compose QML with explicit properties
  flowing down and intents flowing up; QML owns presentation and ephemeral interaction
  state. React is inspiration for composition, not a request for JSX, hooks, Redux or a
  Virtual DOM. Keep the UI low-clutter, contextual, context-menu-first, discoverable by keyboard
  and mouse, readable at supported text scales and accessible without hover alone.
- Agents may implement autonomously within the task's scope. Agent-written code receives
  the same design, behavior, build, test and evidence review
  as any other code. A passing build is not a completed workflow; a benchmark without a
  method is not a performance claim. Question a maintainer proposal when observed code
  or measurements argue against it, and state the evidence.

## Skill routing

Use the smallest useful combination, and inspect the installed skill's own instructions
before invoking it. Skill availability and paths can vary by agent. The
Skill-Library is installed locally when available; its `SKILL.md` descriptions define
scope.

| Task | Useful skill, when available |
|---|---|
| Simplify an overdesigned proposal | `ponytail` |
| Structural code refactor | `principled-refactoring` |
| Broad architecture audit / algorithmic audit | `software-project-analysis` / `algorithmic-project-review` respectively |
| Rigorous second pass on generated artifact | `anti-ai-mediocrity` |
| Disputed external technical claim | `skeptical-research` |
| Product readiness or scope thesis | `software-product-review` / `quantify-ambition` respectively |
| Omarchy end-user desktop configuration | `omarchy` where installed; not ordinary OmaMovie source work |
| PR, issue, release or repository health | the matching focused audit/resolution/release skill only |

Do not invoke several reviewers for a routine edit. Skills supply reusable method, while
Docs and code supply OmaMovie's current decisions and facts.

## Scope and completion

Build the committed workflow before adding effect categories or competitor parity. The
[v0.1 release decision](Docs/implementation-plan-milestone-overview.md) covers only its
validated Intel/software scope; the [first-opening tour](Docs/implementation-plan-v1-onboarding.md)
and bundled editable demo are in v0.1, while contextual tooltip and novice task gates
remain open. AMD/NVIDIA, full HDR and other desired capabilities require their
own evidence. Do not turn aesthetic refactoring into a release gate.

A change is done when its actual behavior and error/fallback path have proportional
checks, affected integration/workload gates pass, documentation states the resulting
scope accurately, and the report names any unverified claim. See
[CONTRIBUTING.md](CONTRIBUTING.md) for commands and [milestone rules](Docs/implementation-plan-milestones.md)
for release criteria.

## Older section references

Older source comments cite sections of the former long guide. Resolve them through the
sources here: §3–5 build/dependencies → `CONTRIBUTING.md`, ADR-0001/0018 and CMake;
§6 time → ADR-0002; §7–9 GPU/media/compositor → ADR-0004/0005/0006 and M2/M3/M12;
§10–11 timeline/UI → M5/M6 and `Docs/ui-design.md`; §12–13 audio/concurrency →
ADR-0003/0020 and M4; §14–15 project/cache → ADR-0007/0009 and M7;
§16 interop → ADR-0008 and `Docs/interop.md`; §18–19 parser/errors → QA and code;
§20–26 agent/process/performance/tests → ADR index, evidence, milestones and
`CONTRIBUTING.md`. Update a touched comment to its direct source rather than extending
this compatibility map.
