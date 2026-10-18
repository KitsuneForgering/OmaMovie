# CLAUDE.md context audit (2026-10-15)

Scope: the former 852-line agent guide, current code and tests in the described paths,
implementation-plan/ADR/UX/evidence indexes, README, and the 20 `SKILL.md` entries in the
local Skill-Library. The current guide is a router; `git show HEAD:CLAUDE.md` preserves
the old text for historical references. This audit records placement decisions, not new
implementation claims. The user's uncommitted stress command was already described by
`docs/spikes/session-stress.md` and `tools/bench/stress/`; it was not discarded as work.

## Former guide disposition

| Former section | Decision | Current source / reason |
|---|---|---|
| 0 status, 24 order, first experiment | OUTDATED / DELETE | Dated milestone and spike history belongs in implementation plan, evidence and git history; many “next” statements had shipped |
| 1 identity, 2 scope | SHORTEN | One product paragraph and bounded scope in the router; changing capabilities remain in plan |
| 3 commands, commits, CI | MOVE TO CONTRIBUTING | `CONTRIBUTING.md`, `make help`, CI and `PKGBUILD` are executable sources; stress/spike invocations live with protocols |
| 4 stack, 5 layout/dependency graph | SHORTEN / MOVE TO ADR | Minimal stack fact retained; `cmake/OmaDependencyGraph.cmake`, include check and ADR-0018 own the enforceable graph |
| 6 time | MOVE TO ADR | ADR-0002 and `libs/base` own rational time and conversion rules |
| 7 GPU/decode/color, 8 media frames, 9 compositor | MOVE TO ADR / OUTDATED | ADR-0004/0005/0006 and M12; old target treated export like a no-readback path while code reads NV12 back |
| 10 timeline, 11 UI, 12 audio, 13 concurrency | MOVE TO DOCS / ADR | M4–M6, UI design, ADR-0003/0014/0020 and code; retain only stable engineering preferences |
| 14 project, 15 cache, 16 interop, 17 SQLite | MOVE TO DOCS / ADR / DELETE | ADR-0007/0008/0009, M7, `docs/interop.md`; speculative SQLite guidance is not a current implementation fact |
| 18 parser safety, 19 logs/errors | MOVE TO DOCS / ADR | QA plan and base/media/project contracts; reusable review method belongs in skills, not an OmaMovie manifesto |
| 20 ADR process, 21 agent rules | SHORTEN / MOVE TO CONTRIBUTING | ADR index and short change protocol retain the actionable part; blanket “ask before” list was too broad |
| 22 C++ conventions, 25 test mechanics | MOVE TO CONTRIBUTING / ADR | ADR-0001, formatting config, tests and contributor guide; Cest macro details remain discoverable in tests |
| 23 performance, 26 philosophy | SHORTEN | Keep measure/algorithm/evidence preferences; numbers and scenarios remain in benchmark protocols |

The older guide had a critical drift: its §7.2 described GPU readback as forbidden in
playback/export, while `exporter.cpp` explicitly reads NV12/RGBA back and
`video_writer.cpp` uploads to a VA-API surface. It also listed `ComputeBackend` and
multi-backend types as architecture without checking their existence. Historical
`docs/analysis/` pages and source comments still cite old section numbers; the router
contains a compact pointer map until touched comments are linked directly to ADRs/code.
Do not mistake that compatibility map for active architectural authority.

## Skill-Library inventory and routing

All entries below are local Skill-Library descriptions, not mandatory steps. Each skill
may have its own supporting references; read those only when applying it. No skill has a
project runtime dependency. Audit/review skills are read-only unless the user separately
authorizes implementation. Installation availability varies across agents.

| Skill | Use / input | Do not use / overlap |
|---|---|---|
| `ponytail` | Smallest complete coding solution, overdesign concern | Not every coding task; use `principled-refactoring` for an actual structural refactor |
| `principled-refactoring` | Behavior-preserving design change in existing code | Not a narrow fix or read-only audit |
| `software-project-analysis` | Broad technical architecture assessment | Not a focused performance or product audit |
| `algorithmic-project-review` | Data structures, complexity and scalability | Not a generic architecture review |
| `anti-ai-mediocrity` | Second pass on generic/generated artifacts | Not routine lint or a full repository audit |
| `skeptical-research` | Deep disputed external claim with sources | Not a simple lookup or internal code fact |
| `software-product-review` | Product usefulness, onboarding and adoption | Not broad engineering analysis |
| `quantify-ambition` | Testable thesis for a large vision | Not ordinary feature estimates |
| `github-project-health` | One repository's technical health and hygiene | Not a PR or specific execution-path algorithm audit |
| `pr-audit` | Independent merge-readiness review | Not a repository health audit or arbitrary implementation |
| `iss-audit` | Read-only issue diagnosis | `github-resolution` handles approved implementation |
| `github-resolution` | Approved ticket through verification/closure | Not initial triage or a broad audit |
| `pr-bump` | Eligible Dependabot batch | Not arbitrary PRs; suspicious updates need `pr-audit` |
| `release` | Explicitly requested release | Not normal milestone work |
| `product-readme` | Product-facing README rewrite | Not internal reference docs |
| `learn-from-work` | User explicitly wants practice/learning | Not an ordinary implementation request |
| `humanizer` | AI-sounding prose rewrite | Not a code/architecture audit |
| `preserve-my-english` | Minimal English correction | Not translation or broad copywriting |
| `blog-reader-panel` | Blog reader reactions | Not product UX review |
| `i-have-adhd` | Explicit ADHD-friendly response request | Not a default formatting rule |

The installed Codex `omarchy` skill is separate from this Skill-Library inventory and
applies to end-user Omarchy desktop configuration, not ordinary OmaMovie source work.
Use the smallest applicable combination. In particular, `software-project-analysis`,
`algorithmic-project-review`, `anti-ai-mediocrity` and `software-product-review` have
different outputs; invoking all four for a routine edit creates duplicate ceremony.

## Code-grounded findings and evidence gaps

- **Export:** software `FrameSource(nullptr)` → Vulkan upload/composite → NV12/RGBA
  readback → CPU `EncodedFrame` → `VideoWriter` row copy and, for VA-API, upload. Existing
  export tests independently decode selected frames/audio and exercise cancellation,
  publication and recovery. The new [transfer diagnostic](../Research/evidence/2026-10-15-export-transfer-baseline.md)
  counts explicit copies/uploads, but still groups GPU NV12 conversion/wait/host copy.
  “Hardware encoder” is implemented on the validated Intel scope;
  “end-to-end zero-copy export” is not implemented or validated.
- **QML:** `Main.qml` defines actions and global window state; `TimelineView.qml` uses
  ancestor `root`/`session`/`actions` while some gestures already use `ClipDrag`. The
  5,000-clip work has recorded latency/evidence. Component extraction must prove focus,
  behavior and long-timeline cost, not merely fewer lines.
- **Session:** owns editor, project state, scheduler, audio, viewer sources, caches and
  multiple job pools; export has a plausible independent lifecycle. The measured
  transport ownership note argues against extracting it now. Map signals and lifetime
  before a controller move; a list of proposed controller names is not evidence.
- **Support limits:** Intel playback/decode and scoped exports have dated evidence;
  AMD/NVIDIA and full HDR do not. 4K60 multistream, long-running resource stability,
  small-window/screen-reader interaction, first-opening comprehension and zero-copy
  encoder input remain unvalidated or planned. Distinguish code presence from the
  support commitment in each public claim.
- **Documentation drift:** README still calls OTIO/FCPXML wholly planned, while M9/code
  implement bounded importers with fidelity gaps. `docs/adr/README.md` still describes
  ADR-0005 on-screen handoff as pending although its body and preview code say otherwise.
  These are independent documentation corrections; neither expands the support matrix.
