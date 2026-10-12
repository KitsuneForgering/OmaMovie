## Conclusion and decision

VEGAS adds an event-based editing reference missing from this research. The most useful
lesson for OmaMovie is to separate source-range selection from sequence placement:
the Trimmer marks in/out and inserts only the selection. Second, show the scope of a
ripple before editing. Switching the media of an event while preserving its position
(takes) merits a pilot after the basic save/export workflow. Sync links and proxies
reinforce existing decisions without requiring a second link or substitute-file model.

**Question:** which VEGAS interactions reduce errors or steps when editing OmaMovie's
target recordings and phone videos? Baselines: library → insert whole file → trim on
the timeline; existing insert/overwrite/ripple commands; ADR-0014 anchors and ADR-0011
transitions. Pilot criteria, set before testing: fewer actions/errors to locate and
insert the desired range; predictable placement and movement; equivalent output,
undo, and reopened project; interface and model cost proportional to the gain. No
user outcome was measured here.

Hypotheses to test:

1. **Source selection:** in/out before insertion reduces timeline trimming on long
   recordings. This weakens if a simple library selection does as well or switching
   source and sequence contexts creates errors.
2. **Visible ripple scope:** showing movement beforehand reduces mistakes with titles,
   music, and markers. This weakens if the current arrows and undo already solve the
   task with the same accuracy.
3. **Takes:** switching between two recordings of the same event without moving its
   cut saves work. This weakens if overwrite is as quick and safe or alignment of
   different sources frequently causes ambiguity.

[Return to vegas-pro.md](vegas-pro.md)
