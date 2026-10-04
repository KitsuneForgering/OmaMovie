# Final Cut Pro: lessons for OmaMovie

> Documentary research, 2026-10-04. Scope: Final Cut Pro **for Mac**, with the
> Apple guide's 12.4 page used where a version is exposed. This describes
> documented behavior, not a hands-on test, usability study, or performance
> comparison. Sources and access limits are recorded below.

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

## What Apple's documents establish

| Mechanism | Documented behavior | Bounded lesson |
|---|---|---|
| Magnetic timeline | A primary storyline ripples around edits. Connected clips move with the primary clip. The Position tool overwrites at a fixed place and leaves a gap at the origin. [Apple guide, “Intro to the Magnetic Timeline”](https://support.apple.com/en-hk/guide/final-cut-pro/verb8fcfc133/mac), “Primary storyline” through “Position tool and gap clips”. | Couple a dependent clip to an anchor, and expose a deliberate way to preserve absolute timing. The attractive language about being faster is vendor opinion, not evidence. |
| Recovery | Changes save automatically; a project can be duplicated; the library database is backed up separately from media. [Apple, “Save and back up projects”](https://support.apple.com/guide/final-cut-pro/save-and-back-up-projects-ver79aa3d71/mac), opening paragraphs. | Separate live autosave, a user-restorable version, and source-media backup claims. OmaMovie has no nested-clip snapshot requirement yet. |
| Media representations | Original, optimized and proxy media are distinct. Transcoding can happen in the background; generated files can be deleted and recreated. Apple instructs users to choose original/optimized before sharing. [Apple, “Create optimized and proxy files”](https://support.apple.com/en-md/guide/final-cut-pro/verb8e5f6fd/mac), “Create” and “Delete” sections. | Model proxy as replaceable media tied to an immutable source identity. OmaMovie should enforce an export-source policy itself rather than rely on the user's viewer setting. |
| Organization | Roles label clip function and support timeline grouping and separate exports. [Apple, “Organize clips by roles”](https://support.apple.com/en-gb/guide/final-cut-pro/verdbd59f7/mac), opening paragraphs. | Useful if source discovery or audio deliverables become frequent tasks; neither is established for OmaMovie's first release. |
| Interchange | Final Cut imports/exports versioned FCPXML; exports from version 1.10 onward use `.fcpxmld` bundles. [Apple, “Use XML to transfer projects”](https://support.apple.com/en-euro/guide/final-cut-pro/verdbd66ae/12.4/mac/26.6), “Import” and “Export”. Apple documents rational time attributes and a DTD. [Apple Developer, “Timing Attributes”](https://developer.apple.com/documentation/professional-video-applications/timing-attributes); [“Creating FCPXML Documents”](https://developer.apple.com/documentation/professional-video-applications/creating-fcpxml-documents). | M9 must pin an actual export version and test semantic mapping. XML validity alone does not establish edit fidelity. |

All Apple pages share one vendor lineage. They establish advertised/documented
contracts, not independent evidence of efficiency or user preference.

## Foundation and fit to this repository

The relevant foundation is an explicit timeline state model: time is rational,
clip identities survive edits, and a command must preserve declared invariants
or fail atomically. [ADR-0002](../adr/0002-time-representation.md) defines the
time representation; [the timeline API](../../libs/timeline/include/oma/timeline/edit.hpp)
states that ripple moves later clips **on the same track only** and exposes
transactional commands. These are verified local contracts. It is an **inference**
that a visual “connection” needs a persisted anchor relation rather than a
one-off UI group: otherwise save/load, undo/redo, and edits outside the UI can
break the relation. The exact anchor policy remains an assumption to resolve.

Define a minimal anchor as `(dependent clip ID, primary clip ID, offset in
sequence ticks)` only after deciding what split, trim, delete, speed change,
locked tracks, and movement across the anchor boundary mean. A surviving
anchor must refer to an existing primary clip. A failed operation should leave
timeline and history untouched; a successful operation should be one undo entry.
Do **not** infer Apple's internal representation from its visible behavior.

OmaMovie's current [implementation plan](../implementation-plan.md) already
includes M7 autosave/relink, M8 proxies and M9 FCPXML evaluation. The new issue
is order: anchor semantics must be settled before the native format is frozen.
The [README](../../README.md) says projects cannot yet be saved or exported, so
the immediate recovery gain is M7 durability, followed by versions only if the
mistake-recovery task justifies them.

## Alternatives, tests, and stopping rules

| Choice | Simpler baseline / cost | Proposed discriminating check (not executed) |
|---|---|---|
| Anchored dependents | Keep track-local ripple and manually group clips. Anchors add edge cases and migration rules. | Construct a short edit with title, cutaway and detached voiceover. Move, split, ripple-trim and delete the primary clip; save/load and undo/redo each action. Accept only if relative timing follows the written policy, locked tracks behave explicitly, and no invalid/dangling anchor survives. In M6, compare mistakes and recovery against manual grouping. |
| Explicit position mode | Reuse existing overwrite/remove commands without a mode. A mode adds state and discoverability work. | At wide/half/narrow layouts, ask users to replace a shot without moving later edits and then restore it. Require the destination, displaced material, and gap to be predictable, including keyboard-only operation. |
| Prior project version | Autosave alone is simpler and covers interruption, but also saves user mistakes. Versions use storage and need retention/restore UI. | Inject interruption during save and reopen; separately make an unwanted edit, autosave, then restore an earlier valid version. Accept only if project integrity and media references are preserved; document that source files require separate backup. |
| Proxy representation | First try bounded decode-ahead, reduced preview quality, and background render. Proxy generation costs time and disk. | On representative 1080p/4K phone and recording fixtures, compare smoothness/tail frame time, seek delay, generation time, disk use and output fidelity at fixed hardware/driver. Accept only for workloads where the end-to-end benefit is material; export must resolve originals or fail clearly. |
| Roles/keywords | Filename/date search and existing tracks are cheaper. Metadata and stem export add persistence/UI work. | Defer until users cannot find source shots or repeatedly need separate audio deliverables in observed tasks. |
| FCPXML | OTIO first has a public adapter/schema path already planned. FCPXML adds versioned bundle, timing and feature-mapping work. | Import actual version-pinned exports covering cut, connected clip, gap, audio and transition. Compare duration, order, anchors and media references; report every unsupported object. No blanket “Final Cut compatible” label from parsing alone. |

This review executed **documentary checks only**: Apple guide passages and the
local model/plan were inspected. No Final Cut installation, exported fixture,
user study, workload benchmark, or crash injection was run. The acceptance
criteria above are proposed before results. No comparative performance or
usability claim follows from this review.

## Source and stopping record

Accessed **2026-10-04**. Apple's Mac guide exposes a 12.4 selector on the
versioned pages linked above; unversioned links are live pages with publication
and update date **unknown**. The browser exposed the cited Apple Support
passages as HTML text. Apple Developer's timing and creation pages were visible
through indexed text, but direct full-text/Markdown access failed; details of
those pages remain provisional until a fixture and pinned DTD are inspected.
The search results for Apple's snapshot and keyword pages exposed descriptive
passages, but the direct pages were unavailable; neither is a decision premise.
Local code and docs were read from the working tree on this date; their state
may include uncommitted changes. Search covered the mechanisms above, not all
Final Cut features or competing editors. Stopping criterion: enough direct
documentation to identify bounded M5–M9 decisions; the biggest remaining
uncertainty is whether anchors improve representative OmaMovie tasks enough to
justify their editing and persistence complexity.
