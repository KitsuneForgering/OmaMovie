# CPU compositor: one tap on the source grid — 2026-10-22

**Classification:** PERFORMANCE + CORRECTNESS. The CPU reference compositor (export without a
Vulkan device, and the continuation of an export whose device is lost) mirrors
`shaders/composite.comp`, but not its 2026-10-21 change (`5ef7f7a`) that converts only the
first bilinear tap when a layer samples exactly on the source grid. Each skipped tap is a YUV
to linear-RGB conversion with three `pow` calls.

Host: i5-1135G7, `intel_pstate` `powersave`, AC. Release build, `export_bench … software` with
`VK_ICD_FILENAMES=/nonexistent.json` (no Vulkan device, so CPU compositing and libx264).
Input: the first 24 frames of `xed_1080p24_bt709_noise-v3.mp4` (1080p, one native-size layer).

| Build | Export (2 runs) | Process CPU per frame |
|---|---|---|
| `36d1f29` (four taps) | 18,757 / 18,803 ms | 895 / 903 ms |
| plus this change | 6,585 / 6,392 ms | 391 / 393 ms |

Outputs are byte-identical (`cmp`): with both weights 0 the mix returns its first tap exactly.
Scaled or transformed layers keep all four taps; `compositor_tests` (CPU against GPU, transforms
included) and the full suite pass. Still about 0.4 s a 1080p frame: this is a fallback, not a
real-time path.

## Rows over the cores — 2026-10-23

No `perf` here and `ptrace_scope` 1, so `export_bench` ran as a child of `gdb` with `SIGINT`
every 0.4 s (786 sampled frames). Where symbols resolved, the main thread sat in
`CpuCompositor::render` → `composite` → `detailed` → `texel` (the per-pixel YUV to linear-RGB
conversion), on one thread of eight. Every output pixel depends only on the source and its own
previous value, so `composite` now splits the layer's rows over `hardware_concurrency()` threads
(at least 16 rows each). Same 24 frames, two runs each:

| Build | Export | Process CPU per frame |
|---|---|---|
| `5b8269a` (one thread) | 6,354 / 6,371 ms | 379 / 382 ms |
| plus this change | 2,330 / 2,298 ms | 527 / 513 ms |

2.75× faster with byte-identical output (`cmp`); total CPU per frame rises with the threads
(hyperthreads share cores). `compositor_tests` pass under TSan with no report.
