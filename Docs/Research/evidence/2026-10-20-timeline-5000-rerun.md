# 5,000-clip edit audit rerun — 2026-10-20

**Classification:** RESPONSIVENESS / regression check. Rechecks the intermittent ~90 ms split
sample (1 of 3 runs) left unexplained by the
[2026-10-14 stall evidence](2026-10-14-timeline-5000-stalls.txt).

`omamovie --m6-timeline-audit 5000` (whole sequence fitted; 20 rounds of seek, select, split,
ripple delete and undo, each timed to the end of the first Qt frame after the call), Release
build of `6230c9f`, ten consecutive runs with a fresh config/cache each. Host: i5-1135G7 /
Iris Xe, Hyprland on eDP-1 1920×1080, `intel_pstate` `powersave`, EPP `balance_performance`,
`balanced` profile, AC.

| Operation | First three runs: p95 (ms) | Worst single sample, all ten runs (ms) |
|---|---|---:|
| Seek | 25.9 / 26.3 / 25.9 | 26.2 |
| Select | 19.6 / 20.7 / 19.9 | 20.7 |
| Split | 40.2 / 38.6 / 40.3 | 40.8 |
| Ripple delete | 41.1 / 33.3 / 34.4 | 41.1 |
| Undo | 33.3 / 33.4 / 33.3 | 33.7 |

Trim drag p95 was 17.0–20.1 ms (max 27.0 ms). No sample in 200 splits came near 90 ms, and
every operation stayed under half of the 100 ms gate. If the old sample still occurred in one
run of three, ten clean runs would happen with probability about 2%, so it is treated as not
reproduced on the current code. Its cause was never identified and no change is attributed;
the audit remains the regression check.

**Recheck after the sorted-search change (2026-10-21, Release of `403ade6`).** Seven runs, same
command and host. The first run, started right after a clean TSan rebuild and the full test
matrix (15-minute load average 4.8), recorded one split at **300.1 ms** (the other 19 of that
run 29.9 ms p50); the next six runs had split p95 38.0-44.7 ms (max of each equal to its p95,
none near 90 ms), undo p95 31.7-33.4 ms, and seek and select p95 under 27 ms. So the
intermittent long split still exists, now once in 17 runs since 2026-10-20 and under load; its
cause is still unknown. The model's per-edit work is small beside the frame time: split latency
did not change measurably with the searches.

**Phase of the long split (2026-10-21).** The audit now prints every sample over the 100 ms gate
split into the synchronous call (Session, the model and the QML bindings it triggers) and the
wait for the end of the frame that shows it. Eleven more runs: one long split, **291.6 ms =
call 32.3 + frame 259.3 ms** (run 1 of 5); no other edit sample over 100 ms (the project open,
save + load + reimport, always is: 120-521 ms). The stall is after the call returns, in
rendering or presenting the frame (render thread, scene-graph sync, Vulkan present), not in
the model or the bindings. Next: time the render thread's phases for that frame.

**Inside the frame (2026-10-21).** The slow-sample line now also gives, in ms after the call
began, when the render thread last began a frame, synced the scene graph, rendered and ended it.
Twelve more runs, one long split: **291.7 ms = call 26.8, then the next frame began at 291.6;
sync, render and end took under 1 ms.** The time went to the render loop not starting a frame
for 265 ms, not to any work. Hypothesis, not verified: on Wayland Qt's render loop waits for
the compositor's frame callback before drawing, and Hyprland delayed it (throttled or busy
window); that would be intermittent and load-dependent, as observed. A check would log the
frame-callback times (Qt's Wayland logging categories) during a run with a long split. (Phase
values from a frame that skipped rendering are left from the previous frame, which is why some
lines show a render end before its start.)

**Cause found (2026-10-21).** `PreviewItem` now logs any viewer composite over 50 ms with its
parts. In 14 runs every run logged exactly one, of 264-1050 ms, almost all of it *setup*: the
lazy `VulkanCompositor::create` on the first composite (render 1-10 ms, display encode under
3 ms). It runs inside Qt's scene-graph sync (`updatePaintNode`), so the UI thread waits; when
that first composite fell on a timed edit, the edit looked long (run 8: a seek of 543 ms beside
a 504 ms composite). Each audit run used a fresh `XDG_CACHE_HOME`, so Mesa's shader cache
was cold every time. With one cache kept across five runs, only the first (cold) logged a slow
composite (288 ms) and **no edit sample exceeded 100 ms in any of the five**.

So the intermittent long split is the audit's own cold start, not the editing path. What users
can meet is a one-time freeze of about 0.3-1 s at the first viewer frame after installing or
updating the driver (cold shader cache). Creating the compositor before the first frame, off
the scene-graph sync, would remove that freeze; not done yet. Audit runs meant to measure edits
should keep the shader cache warm or discard the first composite.
