## Conclusion and decision

The most useful lesson is an **editing invariant**: when a primary shot moves,
dependent titles, B-roll and sound should remain attached to the intended
moment. OmaMovie already has a storyline and commands for ripple and precise
placement, but its ripple operations currently move clips on one track only.
The next step is to specify attachment semantics and test them before persisting
projects. A visible precise-placement mode and recoverable project versions are
smaller, complementary choices. Proxies and FCPXML are later, bounded pilots.
None of Apple's product descriptions proves these choices will improve OmaMovie
users' results; the M6 task comparison remains the product test.

### Question, baseline, and hypotheses

The decision is which Final Cut ideas help a local editor for short recordings
and phone videos on Omarchy without delaying basic save/export. Criteria set
before this review: (1) avoid accidental sync changes and data loss, (2) keep
precise edits possible, (3) fit OmaMovie's existing timeline/project boundaries,
and (4) earn added complexity with a representative task or measured workload.

* **H1, attachment:** explicit dependent-clip anchors will prevent relative
  timing errors during storyline edits. It weakens if linked and unlinked task
  runs have similar errors, or anchor edge cases make editing less predictable.
* **H2, recovery:** durable save plus an accessible prior project version will
  improve recovery from mistaken or interrupted edits. It weakens if crash and
  reopen tests cannot recover a valid version, or the version adds no recovery
  beyond autosave for realistic mistakes.
* **H3, proxy workflow:** a proxy helps only when original playback misses its
  deadline enough to offset transcoding, storage and quality costs. It weakens
  if end-to-end tests show no useful improvement on target media/hardware.

These are **OmaMovie hypotheses**, not claims about measured Final Cut outcomes.

[Return to final-cut-pro.md](final-cut-pro.md)
