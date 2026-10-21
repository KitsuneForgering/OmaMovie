# Export: building the next frame while this one renders — 2026-10-21

**Classification:** PERFORMANCE. Same host, fixture and settings; one variable. Host: i5-1135G7 /
Iris Xe, Mesa 26.2.4, iHD 26.3.5, FFmpeg n9.0.2, `intel_pstate` `powersave`, EPP
`balance_performance`, `balanced` profile, AC. Load average 6–12 during the runs, and `kswapd0`
busy (memory pressure from other work on the machine), so the spread is wider than on an idle
host.

**Bench:** `build/release/tools/export_bench a2160.mp4 out.mp4 [gpu-input]`
(`tools/bench/export-headless`): the app's exporter on a Vulkan device of its own, one clip of
the 4K23.976 240-frame fixture, VA-API H.264, hardware decode (Auto), and no window. Window-based
audits could not run on this session ([hang diagnosis](2026-10-21-export-audit-hang.md)). On the
readback route it matched the window-based audit's export time from earlier the same day
(6.5–6.7 against 6.35–6.6 s). On the GPU-native route it was 5.1–5.3 s against the audit's one
run of 5.7 s.

**Before:** per frame, the export thread evaluated and decoded (*build*, about 10 ms at 4K) and
then rendered (about 11–17 ms) in turn, while the encoder stage took about 2–11 ms on its own
worker. **After:** frame k+1 is built on a worker of its own while frame k renders. After a
device loss the in-flight build is discarded and the next frame is rebuilt.

| Export time, 4K 240 frames | Readback route | GPU-native input (ADR-0023 pilot) |
|---|---:|---:|
| Before (runs) | 6547 / 6686 ms | 5141 / 5312 ms |
| After (runs) | 4606 / 4725 / 4597 ms | 4435 / 4451 / 4383 ms |
| Change (medians) | **−30%** | **−16%** |

Per frame after: waiting for the build 1.3–3.3 ms instead of about 10 ms. Render rose from about
11 to 15–16 ms on the GPU-native route, because decode now runs beside it on the same GPU and
CPU. Process CPU per frame was unchanged (readback 13–15 ms, GPU-native 4.2–4.6 ms): the work is
the same, only overlapped.

**Not measured:** other fixtures and resolutions, effects, a quiet host, and more than three
rounds. Two rounds before and three after give ranges, not confidence intervals.
