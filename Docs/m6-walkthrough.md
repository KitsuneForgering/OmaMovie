# M6 manual task sessions

The M6 items that tests cannot close: task comparisons, the Position-mode decision and the
"edit a short vlog, including keyboard-only" gate ([M6](implementation-plan-m6-ui.md)).
Run on Omarchy with a release build; one person, screen recorded if possible.

**Setup.** `scripts/m6-task-media.sh` (about 2 minutes) writes dialogue-a/b (20 s, a beep
every 2 s), cutaway (8 s, silent), phone-clip (6 s portrait) and long-recording (10 min, a
beep a minute) to `~/Videos/omamovie-m6`. Start each task from a new project.

**First-launch pass for v0.1.** Begin once with a clean application profile (isolated
`XDG_CONFIG_HOME`/`XDG_STATE_HOME`, no existing onboarding preference). Follow or complete
the short tour. Open its Command Palette button, find Import, return to the tour, then
open the demo edit. Mark a range from its third media tile, place it, split or trim a shot,
save a copy and export; check whether the tour made each step predictable. Then perform
T1–T5 below. Record any operation whose meaning remains
unclear, and each missing, wrong or excessive tooltip. Close the tour and find Help,
Command Palette and shortcuts again without prompting. Repeat T5 keyboard-only with the
tour closed. Record whether the tour can be dismissed/reopened and whether a narrow window
keeps every step and exit usable. The tour cannot substitute for a clear ordinary control:
if a common action remains confusing, record a UI issue at that action.

**Record per task:** wall time, corrections (each undo, plus edits redone by hand), any
moment the result surprised you, and every operation you looked for and did not find.

| # | Task | Done when |
|---|---|---|
| T1 | Dialogue with cutaway: A then B alternating 4 times from source ranges (Shift+O, I/O, W); cover one cut with the cutaway (Q). Roll one cut, slip one shot, slide the cutaway, by keyboard and then by Alt / Alt+Shift drag | Beeps stay on picture changes as intended; two-up matched expectations |
| T2 | Ripple scope: delete a storyline clip that has the cutaway connected, then trim a clip before a marker | You predicted from the preview marks what moved, went and stayed |
| T3 | Replacement vs gap: replace 3 s in the middle of A with B, once by overwrite (D), once by lift then connect | Note which you would want a Position mode for |
| T4 | Range first vs whole then trim: take 0:42–1:05 of long-recording and 1–4 s of phone-clip, each both ways | Times and corrections for both ways |
| T5 | Keyboard-only vlog: import all, build 1 minute with a title, a transition, volume change and export | No pointer used; list every blocked step |

**After the session** fill one row per task in the table below, then update the open M6
items with the result, the decision on a Position mode and on an all-tracks ripple, and any
unavailable operation (it stays open rather than counted as done).

| Date | Task | Way | Time | Corrections | Surprises / missing operations |
|---|---|---|---|---|---|
| | | | | | |
