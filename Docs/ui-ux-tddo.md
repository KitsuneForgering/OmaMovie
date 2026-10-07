# TDDO — OmaMovie UI/UX tasks

> Created on 2026-10-04 from the current Qt Quick interface, screenshots from
> `make run-gui RUN_GUI_SMOKE=1`, and comparisons with [UI design](ui-design.md),
> [iMovie](Research/imovie.md), and [Movie Maker](Research/movie-maker.md).
> This document tracks user experience tasks; engineering milestones remain in the
> [implementation plan](implementation-plan.md). A task is complete when its acceptance
> criteria are verified in the interface, not merely when a code test passes.
> P0/P1/P2 indicate user impact; the milestone column indicates when to do the work.

## Goal

Help someone import recordings and videos, assemble and adjust a short sequence, and
understand both the result and the state of their work. Preserve the existing ideas:
library, viewer, and timeline; magnetic storyline; contextual adjustments; and an
Omarchy integrated identity. Test the interaction choices before calling the UI simple.

## P0 — protect work and complete the main workflow

| ID | Task | Acceptance criteria | Milestone |
|---|---|---|---|
| UX-01 | Protect the temporary session when starting another project or closing the window. | When the session contains media or edits, the user makes an explicit choice before losing work; Cancel preserves the library, timeline, and history. An empty session closes without confirmation. Once saving exists, use the actual unsaved changes state. | M6; integrate with M7 |
| UX-02 | Explain that the current project cannot yet be saved or exported. | Projects and Edit communicate the limitation in user language, without milestone codes such as “M7”; “Recent” does not imply projects survive application restarts. | M6; remove the notice in M7 |
| UX-03 | Show the next step after selecting media. | A selected item offers a visible “Add to timeline” action; Insert and Overwrite remain reachable. Mouse and keyboard both work; double click, drag, and shortcuts keep the same semantics. | M6 |
| UX-04 | Complete edit, save, reopen, and export. | A reopened project retains equivalent media, cuts, and adjustments; the user can find Export, follow its progress, and open the resulting file. Persistence and export also meet M7's technical criteria. | M7 |

## P1 — direct editing and everyday usability

| ID | Task | Acceptance criteria | Milestone |
|---|---|---|---|
| UX-05 | Make controls reachable by keyboard focus. | Tab/Shift+Tab reach visible actions and fields; Enter/Space activate them where appropriate; focus is visible; editor shortcuts still work when a text field does not own focus. Complete the Projects, library, adjustments, and timeline workflow without a mouse. | M6 |
| UX-06 | Improve typography, contrast, and density in tiled windows. | Media names, durations, clips, and messages remain legible with available Omarchy fonts and scaling; essential controls do not overlap at 900 px or 640 px. Compare screenshots with real media, long names, and light/dark themes. | M6 |
| UX-07 | Manipulate framing in the viewer. | With Crop open, visible handles allow moving and resizing the image or crop; the gesture previews its result, creates one undo entry, and numeric controls remain available for precision. *Image move/scale implemented 2026-10-06 (GUI smoke); crop handles open.* | M6 |
| UX-08 | Improve orientation in the timeline. | Drag and trim gestures show the destination before release; zoom keeps the point under the cursor; snapping has a visible state and can be disabled. Audio controls and behavior do not imply that detached lanes follow storyline ripples while they do not. *2026-10-06: clips connected to the storyline (ADR-0014) do follow it and show a stem; unconnected lane clips still do not.* | M6 |
| UX-09 | Make Omarchy recordings a first class entry point. | Projects shows recent recordings; Edit opens one on the storyline. Recordings without audio and files still being written have understandable states. Respect the recording locations specified in M6. *Implemented 2026-10-06 (Projects screen, GUI smoke); user sessions not run.* | M6 |

## P2 — evaluate before expanding the interface

| ID | Task | Acceptance criteria | Milestone |
|---|---|---|---|
| UX-10 | Compare the current adjustment drawer with a side inspector. | Users perform the same color, crop, and volume tasks in both variants at wide, tiled, and narrow widths. Record completion, mistakes, searches for controls, and space left for the viewer before deciding. | M6 |
| UX-11 | Evaluate a Movie Maker inspired storyboard. | Compare storyboard, ordinary time zoom, and minimap using clips of unequal duration, gaps, transitions, and detached audio. Implement storyboard only if users can locate cuts, reorder, undo, and return to precise editing without losing context or causing unexpected edits. | M8 |
| UX-12 | Add intent presets once connected layers exist. | Picture in picture and other M8 presets create editable results; the UI explains the outcome without requiring the user to understand tracks, transforms, and masks before applying it. | M8 |

## Shared validation

1. Use the same files and tasks at 1600, 900, and 640 px: import two videos, add the
   second, trim, adjust framing and volume, undo, and redo. Include save, reopen, and
   export in M7.
2. Repeat with mouse and keyboard only. Record task completion, errors, searches for
   commands, and unexpected changes to another lane.
3. Compare the contextual/magnetic interface with the alternative in UX-10. Keep
   screenshots, version, window size, theme, and observations. Automated smoke tests
   show that commands work; user sessions show whether people can find them.

## Suggested order

Start with UX-01–03 and UX-05 to protect work and unblock the basic workflow. Then
address UX-06–09 and compare UX-10. UX-04 completes the M7 workflow. UX-11–12 remain
M8 hypotheses and do not block the first usable release.
