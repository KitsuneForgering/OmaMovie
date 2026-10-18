# CapCut: documented features and untested preferences

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

Editable timed captions and speed curves are documented feature references. They do not
establish market dominance, universal demand, or the right order of OmaMovie milestones.
Offline/no-account operation remains a project preference, not proof of competitor failure.

## 2. Inspected product evidence

[CapCut's guide](https://www.capcut.com/resource/how-to-use-capcut) describes speed curves
and editable presets alongside effect keyframes. [Its caption page](https://www.capcut.com/tools/auto-caption-generator)
describes generating, correcting and styling timed text. These are promotional primary
sources: they establish advertised workflows, not accuracy, completion times or plan
availability for every region/app/version.

The former price-doubling, bans, perpetual-license/draft claims and billing/crash complaints
were based on commentary, with no verified applicable policy/jurisdiction or representative
sample. They are removed from active guidance. No legal conclusion is made by this audit.

## 3. Foundations: time mapping

Speed is the derivative of source position with respect to timeline time. If timeline time
is `t` seconds and dimensionless speed is `s(t)`, source time is
`m(t) = m(0) + integral_0^t s(u) du`. Merely interpolating source timestamps is not always
the same as interpolating speed; define the interpolation and endpoint duration explicitly.
A frozen segment has a duration despite zero source displacement. Reverse playback needs
a decreasing source map and audio policy. Exact endpoints do not make arbitrary curve
samples exact rationals. See the audit calculation and ADR-0002 review note.

## 4. Implications for OmaMovie

| Choice | Supported application | Pending dependency |
|---|---|---|
| Captions | First-class timed text; SRT/VTT pilot in M8 | Timing/layout/encoding/round-trip tests |
| Speed curves | Explicit TimeMap domain, rounding and interpolation | Complementary ADR and reverse/freeze tests |
| Intent presets | Normal editable commands/parameters | Discoverability/recovery in M6/M8 |
| Resizable canvas | Define transform anchoring before implementation | Coordinate ADR, portrait/landscape tests |
| Transcription | Separate generator of editable timed text | Concrete local use case; remains non-goal initially |

Imported captions can exist without speech recognition. A preset UI does not require cloud
stock, account infrastructure or a separate project model.

## 5. Proposed discriminating test

Compare preset-first with parameter-first controls on identical simple tasks (caption
correction, slow motion, overlay) and mixed-duration media. Record completion/errors, undo,
output timing and ability to adjust after applying a preset. The claim would weaken if
presets hide important changes or slow experienced users without helping beginners.
No task study or caption-accuracy benchmark was executed. Defer high-cost automation until
basic editing proves useful; no feature inherits priority merely from CapCut advertising it.
