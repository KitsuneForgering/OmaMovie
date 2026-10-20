# Timeline evaluate lookup scaling — 2026-10-19

**Classification:** PERFORMANCE / measured, no change needed. Closes the *derived scaling
risk* from the [source-path review](../../implementation-plan.md#architecture-and-release-discipline):
`timeline::evaluate` resolves each track by a second linear track search (`clip_at`) and
each active clip's media by a linear `Timeline::find_media` scan, approximately
`O(T² + T log(C+1) + A·M)` per frame before decoding, effects and pixel work.

`tools/bench/evaluate-scaling/run.sh` builds a synthetic timeline against the Release
`liboma_timeline.a` (GCC 16.2.1, `-O2`) and evaluates every frame five times. Each track
holds back-to-back one-second video clips; media IDs are assigned highest first so the
media scan reaches the end of the vector. Host: i5-1135G7, `intel_pstate` `powersave`
governor (EPP `balance_performance`, `balanced` power profile, AC; read after the sweeps,
not changed during the session), desktop session idle, working tree `ad276a9` plus this benchmark. Three sweeps were run; the
table gives the range of the per-sweep medians (ns per evaluated frame).

| Tracks | Media | Clips/track | Median ns/frame (3 sweeps) |
|---:|---:|---:|---:|
| 1 | 1 | 100 | 347–878 |
| 3 | 3 | 100 | 847–2001 |
| 3 | 3 | 5000 | 1031–2928 |
| 3 | 2000 | 100 | 3576–6022 |
| 8 | 500 | 100 | 2982–7023 |
| 16 | 500 | 100 | 5915–11964 |
| 32 | 2000 | 50 | 25552–49876 |
| 64 | 2000 | 25 | 51178–93971 |

The first sweep ran faster than the later two under the same governor; frequency
scaling is not controlled, so compare shapes, not single numbers.

**Reading.** Clip count is logarithmic as designed (5,000 clips ≈ 100 clips). The media
scan and track count are visible: 2,000 media entries cost about 3× three entries on
three tracks, and 64 tracks with 2,000 media is the worst case at ≤ 94 µs. That is about
0.6% of a 60 fps frame period (16.7 ms) and about 20 s of CPU across a one-hour 60 fps
export (216,000 frames), against exports measured in minutes. A realistic personal or
creator project (≤ 16 tracks, ≤ 500 media) stays near or below 12 µs per frame.

**Decision.** No track or media index. Revisit only if a profile of a real playback or
export run attributes measurable time to `evaluate`, or if supported scope grows past
these sizes. The repeated media lookup in `build_viewer_frame` was not benchmarked; it
adds one more scan of the same kind per layer, so it is expected to be of the same order,
and it is left alone. This benchmark covers lookup cost only; it does
not measure decode, effects, compositor, audio mixing or QML.
