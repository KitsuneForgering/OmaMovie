# iMovie: documented interactions and UX hypotheses

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

Contextual adjustment controls, editable named overlays and skimming are useful references.
They support prototype choices, not a finding that OmaMovie's proposed UI is already easy.
The claim that iMovie shares Final Cut's internal engine is withdrawn from architectural
justification: the inspected Apple guide establishes transfer, not implementation identity.

## 2. Documented behavior

[Apple's Mac guide](https://support.apple.com/en-nz/guide/imovie/welcome/mac) lists trimming,
clip color, skimming, ratings, multiple-clip effects and conversion of trailers to movies.
Its examples put adjustments near the viewer. These are documented interactions; their
presence does not quantify discoverability or learnability for Omarchy users.

[Sending to Final Cut](https://support.apple.com/guide/imovie/send-projects-to-final-cut-pro-movcbf7e2a3f/mac)
copies project/media and maps at least one audio adjustment to a Gain filter. Trailers must
first be converted. This proves a supported workflow with conditions, not shared code,
unlimited capability, or lossless mapping of every effect.

## 3. Foundations and model

Separate presentation from persisted edit semantics. A contextual drawer changes visible
controls; it must not become the owner of clip parameters. Named operations may emit normal
model commands; that is an OmaMovie design inference, not a description of Apple's internals.

Magnetism/ripple is a chosen editing policy over generic tracks. Specify linked audio,
connected overlays, gaps, locked tracks and undo transaction boundaries before implementing
it. A single simple-looking sequence can hide surprising moves on other tracks.

## 4. Alternatives and proposed test

Compare contextual drawer with a fixed side inspector; compare ripple default with ordinary
track placement. M6 tasks: import, split, trim, overlay, gain and undo at wide/half/narrow
sizes using mouse and keyboard. Record completion, mistakes, recovery, hidden-control
searches and surprise track movements. Prefer a design only after representative tasks
show no required action is inaccessible and improvement does not break precise editing.
No usability test was executed by this review.

Skimming requires cancelable decode/cache work, but every hover need not trigger a full seek.
Compare cached thumbnails with live decode; track request age and stale output, not just FPS.

## 5. Removed arguments and application limits

The old iMovie '08 history/reaction narrative was not checked against contemporary primary
records and no longer proves a universal prohibition on removing features. Preservation of
shipped capability is a product preference; removal may need explicit migration/deprecation.
No Mac/iOS feature list is treated as interchangeable. Shortcut choices in `ui-design.md`
are OmaMovie assignments, not a certified translation of all Apple shortcuts.
