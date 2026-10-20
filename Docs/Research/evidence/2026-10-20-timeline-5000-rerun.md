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
