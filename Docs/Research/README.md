# Research

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

The architecture remains a defensible direction, subject to driver, synchronization,
color and workload checks. The product positioning and UX proposals remain hypotheses.
The 2026-10-03 review replaces the previous unsourced market rankings and broad hardware
claims; it does not certify the implementation or complete the pending spikes.

## Reading order

1. [Skeptical review](skeptical-review.md): hypotheses, corrections, foundations and gates.
2. [Source register](sources.md): documents actually inspected and their limits.
3. [Hardware strategy](hardware-strategy.md): bounded application to the pipeline.
4. [Implementation plan](../implementation-plan.md): milestone work and acceptance criteria.

## Documents

| Document | Scope |
|---|---|
| [Vulkan](vulkan.md) | API contracts, video capabilities, FFmpeg, Qt and libplacebo |
| [CUDA](cuda.md) | NVIDIA runtime/build distinction, interop and proposed comparison |
| [OpenCL](opencl.md) | Scope exclusion; counterevidence to an impossibility claim |
| [Hardware strategy](hardware-strategy.md) | Device ownership, selection, experiments and fallbacks |
| [Omarchy integration](omarchy-integration.md) | Installed-file observations and version-sensitive adapters |
| [Premiere](premiere-pro.md) | Editing, color and importer hypotheses |
| [Final Cut Pro](final-cut-pro.md) | Anchored editing, precise placement, recovery, proxies and bounded FCPXML |
| [iMovie](imovie.md) | Contextual controls and transferable UX proposals |
| [Movie Maker / Clipchamp](movie-maker.md) | Historical limits and current documented interactions |
| [Resolve](davinci-resolve.md) | Versioned codec evidence and limits of competitive inference |
| [CapCut](capcut.md) | Documented captions/curves; unmeasured audience preferences |
| [Other editors](other-editors.md) | Credible baselines and bounded comparisons |
| [GPU effects](gpu-effects.md) | Grading (CDL, curves, 3D LUTs), multi-pass cost, keyframes; measured locally |
| [Post-production effects and catalog](post-production-effects.md) | Core effect choices from editor research, candidates to remove, and staged third-party marketplace |
| [Agent-assisted editing with MCP](agentic-mcp.md) | Local project tools, bounded edit proposals, approval, protocol limits and pilot gates |
| [Long-form editing](long-form-editing.md) | Project size versus duration, timeline/UI scaling, proxies, preview cache, recovery and export gates |

## Evidence rules

- **Documented** means a cited passage or local API contract supports the stated scope.
- **Observed locally** describes the installed files or a named experiment, with versions.
- **Hypothesis / inference** names a proposed explanation or deduction, with a disconfirming check.
- **Preference** records a product/engineering choice; competitor behavior cannot prove it.
- **Pending** means the decisive check has not run. A checkbox for implementation is not validation.

Sources are linked beside claims. Multiple vendor pages from one organization are one
lineage, not independent measurements. Marketing pages establish advertised features only.
Unverified release details, prices, legal interpretations and anecdotal complaint rankings
have been removed from active guidance; git history retains the earlier text.

## Supported synthesis

GPU compositing, explicit time/ownership, bounded work and recoverable project persistence
have technical reasons independent of competitor reputation. FFmpeg and Qt expose useful
contracts, but their coexistence must be tested (gate Q1). Intel S1/S2 reports support a
bounded Intel path; AMD/NVIDIA and hybrid support remain unvalidated.

The fixed layout, contextual drawer, magnetic policy and editable presets are product
choices. Compare them with a conventional track/inspector baseline in M6. A minimap and
semantic storyboard zoom are alternatives to evaluate, not requirements proved by history.

OTIO is a reasonable first interchange pilot because its schema/library are public. This
provides no promise of effect fidelity or lossless conversion of arbitrary time values.
Keep proprietary import read-only and fixture/version scoped. No evidence here establishes
an empty Linux market, demand for replacing Kdenlive, or professional parity with Resolve.
