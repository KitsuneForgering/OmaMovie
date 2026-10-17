# Movie Maker and Clipchamp: bounded UI references

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

A storyboard projection and contextual properties remain prototype candidates. The old
claim that Movie Maker 2012 zoom changes equal-width clips into duration-scaled rectangles
was not verified in a primary record. Do not implement semantic zoom on that historical
claim alone.

## 2. Current documented baseline

[Microsoft's Clipchamp timeline guide](https://support.microsoft.com/en-us/clipchamp/how-to-work-with-the-timeline-in-clipchamp)
describes dragging library assets onto the timeline, rearranging, trimming, multiple assets
and properties such as audio. It establishes an interaction baseline, not causal evidence
that a similar arrangement will make OmaMovie usable.

Library/viewer/timeline/properties is one way to make selection-dependent tools reachable.
The existing OmaMovie drawer remains a product choice; compare a side-panel alternative
rather than declaring a layout validated through visual resemblance.

## 3. Historical evidence boundary

The earlier version-by-version dates, format/export restrictions and removal/restoration
claims came mainly from secondary summaries. They were not needed to choose the next
engineering step and have been removed from active guidance. The attempted Microsoft
Windows Essentials URL was unavailable in this audit (source register).

Neither discontinuation nor a feature's disappearance establishes that architecture caused
product failure. Treat that causal story as unsupported, not a foundation for modularity.

## 4. Storyboard hypothesis

An equal-width clip projection can help ordering but destroys the correspondence between
screen distance and timeline time. It needs explicit transition into duration-scaled mode,
a clearly mapped playhead, and gestures whose effects remain predictable. Free overlays
and split audio make a single-row storyboard especially ambiguous.

Compare conventional time zoom, a minimap, and semantic storyboard in M6/M8. Use identical
projects including unequal durations, multiple tracks, gaps and transitions. Accept only if
users can locate the same cut, reorder/undo without changing unintended tracks, and return
to precise time editing without losing context. Measure errors and navigation work; do not
assume fewer modes or fewer pixels is automatically simpler. Test is proposed, not run.

## 5. Application

Keep storyboard as an M8 evaluation item. Keep broad media support, installation usability
and recoverable projects for their direct product value, independent of unverified history.
Prices, account/plan restrictions and browser performance were not measured and do not
support an OmaMovie performance advantage.
