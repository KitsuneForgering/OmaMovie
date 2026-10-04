# Evidence recheck — 2026-10-04

This is a bounded recheck of local evidence, not a rerun of the S3/S5 or GPU-effects benchmarks.

## Reproduced

- `python3 Docs/Research/evidence/2026-10-04-grading-numerics.py` with NumPy 2.5.3 exited 0. Every printed value matched section 2 of [gpu-effects.txt](2026-10-04-gpu-effects.txt) to the displayed precision: LGG/CDL `6.43e-15`, steep-curve table errors `2.25e-03` (256) and `1.58e-04` (1024), 33³ tetrahedral LUT maximum error `2.12e-02`, and zero reported grey-axis spread. These are synthetic numerical checks; they are not an independent color reference.
- Independently recomputed the capacity arithmetic in [2026-10-03-calculations.txt](2026-10-03-calculations.txt) using Python's standard library: 4K RGBA16F is `63.28125 MiB`, one full-frame transfer at 60 fps is `3.981312 GB/s`, and three streams with eight frames each occupy `1518.75 MiB`, excluding other allocations.
- `make test` in the current debug tree exited 0. The [complete output](2026-10-04-make-test.txt) contains the eight suite summaries: base 118, gpu 41, media 652, compositor 164, audio 89, timeline 292, playback 49, project 38 Cest-reported checks passed. One compositor hardware-decode branch printed `(skipped: no hardware decode here)`; a green suite is not validation of that path. The first attempted rerun exited 2 because `compositor_tests` was temporarily non-executable during a rebuild; each test binary then exited 0 individually, and the final `make test` passed.

## Documentary checks and limits

- Rechecked the Q1 source-level premise against the [Vulkan `vkGetDeviceQueue` reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceQueue.html) (Valid Usage, `flags-01841`, accessed 2026-10-04) and [Qt 6.11.2 `qrhivulkan.cpp`](https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/gui/rhi/qrhivulkan.cpp) (imported-device branch, lines 832–862, accessed 2026-10-04): the former requires a zero-flag queue, and the latter calls `vkGetDeviceQueue` for an imported device. This corroborates the historical mismatch in the skeptical review; it does not establish what patches the installed Qt binary contains or validate concurrent live playback.
- [S3's method](../../spikes/S3-decode-paths.md) had said SVT-AV1 preset 11; [its script](../../../tools/spikes/s3-decode-paths.sh) uses preset 12. The method was corrected to 12. The S3 throughput, CPU, seek, PSNR and fallback results were **not rerun**. Its saved output reports summary statistics, not individual trial timings; the input media is not retained here.
- [S5's results](2026-10-04-s5-minimal-compositing.txt) report two runs per path, with a reproducible harness but no retained per-frame timings or input media. In the recorded hardware runs, 3 and 4 of 600 sequential decode-plus-composite iterations exceeded 16.67 ms. The harness does not measure Qt presentation, audio or the viewer scheduler. These results support an S5 library pilot, not an end-to-end 1080p60 playback claim.
- The [GPU-effects benchmark](2026-10-04-compositor-bench.cpp) times synchronous compositor rendering of one decoded frame. The numerical recheck does not reproduce its GPU timing, nor establish AMD/NVIDIA behavior or independent color accuracy.

## Next discriminating check

Measure 1080p30/60 **new-frame time at Qt presentation** for the same declared media and scene, recording the negotiated decode path, dropped/repeated frames, p50/p95/p99/max, A/V drift and the render-thread stalls over a sustained run. Compare software decode/upload with the admitted hardware path on the same output. This is the M4 gate needed before choosing a viewer default or claiming real-time playback; isolated S3/S5 throughput cannot decide it.
