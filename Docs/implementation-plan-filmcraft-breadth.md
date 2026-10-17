### FilmCraft breadth check (2026-10-11)

[FilmCraft](https://github.com/storytold/filmcraft) from StoryTold is a closer open-source
engineering comparator than a commercial feature list. Its published repository describes
roll/slip/slide and three-point editing in the UI, a shortcut editor, animated effects,
audio track routing, several delivery/interchange formats and a shared command engine.
OmaMovie's core already implements several timeline operations, exact time, GPU compositing,
project recovery and a shared action path, but many operations have no complete editor
workflow yet. The gap is real in user-visible breadth, even though FilmCraft's own
[gap inventory](https://github.com/storytold/filmcraft/blob/main/docs/gaps.md) calls out
Linux runtime failures, missing real-media acceptance coverage and output-fidelity work.
Its ~86% feature-breadth and ~56% work-readiness numbers are self-estimates against
Premiere, not measurements against OmaMovie. We did not build or run FilmCraft here.

| FilmCraft workflow | OmaMovie evidence | Decision |
|---|---|---|
| Source-to-timeline and precise trims | Source playback and marked placement exist; roll/slip/slide have commands, keyboard actions and storyline pointer gestures, but one visible task plus decoded-content checks remain open | Finish that task before expanding effect count (M6) |
| Audio and titles | OmaMovie has clip EQ/noise controls and basic generated titles; no track routing or rich text layout | Prioritize retimed-audio correctness; pilot stems and typography only for demonstrated tasks (M8/M9) |
| Deliverables and interchange | OmaMovie exports four MP4/Matroska/WebM profiles and SRT and plans OTIO/FCPXML; FilmCraft reports additional professional codecs and AAF/OMF | Finish reliable output/snapshot behavior, then validate one named hand-off format rather than counting extensions (M7/M9) |
| Runtime quality | OmaMovie has Intel GPU/Wayland tests and long-form clip-count evidence; FilmCraft reports incomplete Linux and real-media coverage | Keep same-hardware, independently decoded outputs and workflow tests as the comparison gate (M10) |

Do not infer that Rust itself explains the breadth difference: language, staffing, scope
and maturity were not controlled. A meaningful follow-up uses the same generated media
and task script on both installed Linux builds, records completion/corrections, p95 UI
latency, dropped frames, export time and independently decoded output, and notes the
exact revisions, GPU, driver and codec. Until then, feature existence is a sourced
comparison; relative speed and fidelity are unknown.

Milestone checkboxes record implemented deliverables. Validation requires separate evidence.
The [2026-10-12 engineering assessment](analysis/omamovie-2026-10-12.md) compares
implementation, owned media surface and test gates separately from feature breadth;
neither editor was built or run in an equivalent task here.
S1/S2 and M0/M3 numbers below are prior reports, not rerun by the documentation audit.
New acceptance criteria are proposed pilot criteria set before future measurements; changes
to them must be recorded, rather than choosing thresholds after seeing results.

[← Implementation plan](implementation-plan.md)
