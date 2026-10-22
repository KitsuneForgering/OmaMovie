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
