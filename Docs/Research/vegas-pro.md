# VEGAS Pro 23: source selection, ripple, and alternate takes

> Documentary review with skeptical-research, 2026-10-07. Scope: official VEGAS Pro
> **23** help. Links and access limits are in the [source register](sources.md).
> This describes documented behavior, not a hands-on test, user study, or performance comparison.

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

## What the documentation establishes

| Mechanism | Documented behavior | Bounded application to OmaMovie |
|---|---|---|
| [Trimmer](https://help.magix-hub.com/video/vegas/23/en/content/topics/2-window/window_trimmer.htm), “Selecting data” and “Adding media to the timeline” | Selects source in/out before timeline placement; supports insertion from the cursor, three-point editing, and subclips. | Pilot a source viewer with I/O and insert/overwrite. Start with `ClipSource` carrying the selected range; reusable subclips are a separate decision. |
| [Post-edit ripple](https://help.magix-hub.com/video/vegas/23/en/content/topics/7-edit/post_edit_ripple.htm), “Ripple types” and “Applying…” | Can affect the edited tracks or all tracks; broader modes also move markers and automation. Automatic mode changes the visual highlight. | Specify what each command moves and preview it during drag. Current OmaMovie commands move only the edited track and anchored dependents; do not call that all-track ripple. |
| [Sync links](https://help.magix-hub.com/video/vegas/23/en/content/topics/7-edit/using_sync_links.htm), introduction and creation | One-way relation: a dependent follows the main event, but moving it does not move the main event. | Compare with ADR-0014. OmaMovie's anchor follows a position in the primary media and covers the central case; its split, trim, and deletion rules differ. Do not infer complete equivalence or copy internal representation. |
| [Takes](https://help.magix-hub.com/video/vegas/23/en/content/topics/7-edit/take.htm), “Creating”, “Choosing”, and “Switching” | An event can contain alternate sources and an active take used for playback/render. | Pilot media substitution that preserves event start, duration, and effects. Define alignment, available duration, audio, and project references first. |
| [Automatic crossfades](https://help.magix-hub.com/video/vegas/23/en/content/topics/7-edit/autocrossfades.htm), “Creating a crossfade” | Overlap on one track can create a crossfade when the option is active. | Compare the gesture with OmaMovie's explicit cut transition. Silent overlap would change current storyline semantics. |
| [Adjustment events](https://help.magix-hub.com/video/vegas/23/en/content/topics/7-edit/adjustmentevents.htm), introduction and “Limiting…” | A time-bounded event applies effects to videos below it and can restrict affected tracks. | Consider only after the per-clip effect stack and a real task requiring one correction over several shots. Track scope, composition order, and export must be explicit. |
| [Video proxies](https://help.magix-hub.com/video/vegas/23/en/content/topics/5-preview/creating_intermediate_files.htm), note and “Preview Quality” | Proxies serve preview; the guide says rendering uses originals. | Reinforces the planned M8 separation, without proving a proxy helps OmaMovie's hardware or should be generated automatically. |
| [Multicamera editing](https://help.magix-hub.com/video/vegas/23/en/content/topics/7-edit/editing_multicamera_video.htm), introduction and “Previewing multiple takes” | Choose takes during playback or pause and split the event at a switch; the help warns that many previews limit frame rate. | Keep multicamera separate from a simple takes pilot; it needs synchronization, multiple decoders, and measured preview deadlines. |

These pages share **one vendor lineage**. They establish documented behavior, not user
preference, Linux performance, or internal implementation details.

## Local fit and alternatives

The foundation is OmaMovie's timeline state and command contract: source range,
sequence position/duration, media identity, links, transactions, and persistence are
distinct. [ADR-0002](../adr/0002-time-representation.md) requires rational times;
[edit.hpp](../../libs/timeline/include/oma/timeline/edit.hpp) accepts a `ClipSource`
with `source_in` and duration and exposes insert/overwrite/ripple. [ADR-0014](../adr/0014-connected-clips.md)
defines anchors, and [ADR-0011](../adr/0011-transitions.md) defines cut transitions.
These are observations of local code and decisions.

**Inference:** a minimal Trimmer can use existing commands without a new clip type.
The selection state belongs to the source until insertion; the project stores the
range actually inserted. Validate source bounds, sequence-frame rounding, audio/video
selection, and incomplete media. Reusable subclips or persistent takes change the
native format and require an ADR and migration. VEGAS's interface does not reveal
its underlying data structure.

Final Cut research emphasized anchors for dependent clips. VEGAS documents a one-way
link alternative, but OmaMovie already has more specific anchors. Visibility and
predictability are now the useful questions. All-track ripple and its marker rules
are separate from current track-local ripple and need their own command and preview
if the pilot justifies them.

| Pilot | Baseline and cost | Acceptance or abandonment criterion (not run) |
|---|---|---|
| Source viewer with I/O | Insert whole media then trim; a second playback context costs space and focus. | Compare a long recording and short phone clip with mouse, keyboard, and narrow window. Count actions, elapsed time, in/out mistakes, and recovery. The inserted range must survive undo, save/reopen, and export. Drop the viewer if it yields no reproducible gain or confuses placement. |
| Ripple scope | Current movement arrows, anchors, and undo; all-track scope adds locked-track, gap, and marker cases. | With B-roll, title, speech, music, and a marker, identify every affected object before trim/delete. If broader ripple is added, result, undo, and reopen must preserve intended offsets; no unindicated track may move. |
| Alternate take | Overwrite and undo already replace media; persistent takes need format, UI, and duration policy. | Compare two aligned narration or camera recordings. Switching preserves range, effects, links, and duration or explains refusal; preview and export use the same active source. Compare errors and steps with overwrite before accepting a new model. |
| Overlap crossfade | Explicit cut transitions exist; overlap changes storyline semantics. | Make the same dissolve both ways and compare mistakes and later adjustment. Adopt only if overlap reduces effort without creating unintended transitions. |
| Adjustment event | Copying an effect or preset may suffice. | Correct several shots while excluding an overlay. Inspect preview/export frames, scope, ordering, undo, and save/reopen. Keep per-clip effects if the task shows no gain. |

No `.veg` import is proposed: no project specification or fixture was inspected.
Any future interchange work should start with legitimately produced, versioned files
and report losses as M9 requires. Audio buses, nesting, and multicamera are future
task references, not dependencies of the v0.1 workflow.

## Verification and limits

**Local work:** read `edit.hpp`, ADRs, and the plan to map proposals to current state;
no VEGAS execution or new OmaMovie test. **Documentary work:** the official VEGAS
Pro 23 HTML help pages above were read on 2026-10-07. “Last modified” showed
2025-12-08 for ripple, takes, sync links, crossfades, adjustment events, and proxies;
the dates of the others are unknown. The decisive uncertainty is whether target
users gain time or make fewer mistakes; vendor documentation cannot answer it.
