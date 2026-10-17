# Premiere: editing, color and interoperability evidence

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

Professional editing operations and explicit color conversion are useful references.
Premiere does not establish that GPU acceleration alone guarantees real-time playback or
that OmaMovie must implement every advanced command before the first usable cut.

## 2. Documented color model

[Adobe's color overview](https://helpx.adobe.com/premiere/desktop/correct-color/set-up-color-management/about-color-management.html)
(updated 2026-01-07) distinguishes input interpretation, working processing and output for
monitoring/export, with configurable SDR/wide-gamut workflows and overrides. This supports
separating these stages in OmaMovie. It does not establish a universal ACEScct working space
or a particular preset count for every version. Those old specifics are withdrawn.

OmaMovie's linear BT.709/RGBA16F implementation is an SDR proposal pending ADR-0006. Compare
against independently generated vectors, not only the CPU implementation of the same math.
Detecting PQ/HLG metadata without transforming it is not HDR support.

## 3. Editing and playback

The [Adobe Help guide](https://helpx.adobe.com/premiere-pro/using/color-management.html)
indexes trim, proxy and hardware-acceleration documentation. This audit read the color
passage and guide index, not every editing feature article. Detailed Premiere behavior,
performance and format matrices remain outside verified coverage here.

For OmaMovie, ripple/roll/slip/slide names describe different invariants and need command
semantics and undo tests. Scope them progressively. Three-point editing, dynamic trim and
source patching are future candidates; familiarity with Premiere is not evidence that the
first audience requires them all. CPU/GPU effect capability alone cannot predict whether a
section meets a deadline: workload, transfer and scheduling also matter.

## 4. Import hypothesis

Some `.prproj` files have been reported as compressed XML. This audit inspected no exported
Premiere fixtures or current schema guarantee, so the universal statement is withdrawn.
Keep it a version-scoped read-only investigation after public interchange formats.

Needed evidence: legally produced project pairs with known edits, file signatures and
bounded decompression/parser behavior. XML accessibility does not imply semantic simplicity:
references, nested sequences, third-party effects and time semantics can still be unmapped.
Declare support by tested format/version/feature, never by product name alone.

## 5. Alternatives and decisions

Proxy/offline workflows and cached rendering are credible alternatives when native media
misses deadlines. Compare total costs and fidelity instead of treating transcoding as a
failure. A GPU-first pipeline still needs a verified CPU/reference path and error recovery.

Keep atomic save, autosave and fuzzing because lost work is costly. No representative
crash-rate or complaint-frequency study was read, so the previous competitor instability
ranking and claims of a resulting competitive advantage are withdrawn. Product naming,
workspaces counts, release-specific AI features and broad camera claims do not guide the
current implementation and are not retained as facts.
