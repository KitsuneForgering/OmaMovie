# RC correctness-gate pilot — 2026-10-17

**Scope:** Intel Iris Xe (Tiger Lake), Intel iHD 26.3.5, FFmpeg n9.0.2, MLT `melt`
7.42.0 (the engine used by installed Kdenlive 26.08.2). The checkout was dirty at
`0b2a8ab`; this is a pilot of the gate, not evidence for a frozen RC SHA. MLT renders
were driven by `melt`, not the Kdenlive user interface. Only one comparison round was
run, under varying system load, so the wall times below do not support a speed ranking.

## Results

- `make BUILD=release test` outside the sandbox: 14/14 CTest suites passed in 35.09 s.
  The same run inside the sandbox stalled in the Vulkan compositor test; the unrestricted
  run passed that suite in 6.00 s. `OMA_PROPERTY_RUNS=3000 OMA_PROPERTY_STEPS=200
  build/release/tests/timeline_tests` passed all 459 test cases, including 600,000 seeded
  edit steps, in a separate run.
- The final shared fixture has 479 decoded 1920×1080 frames at 24000/1001, explicit
  BT.709 primaries/transfer/matrix and limited range, and seeded pink-noise audio. SHA-256:
  `b555007fdc2c34c94678969900a2beb2f5894c2f809f4db0b29e9dc5f69df8ed`.
  An earlier fixture had BT.709 matrix but `unknown` primaries and transfer despite
  encoder command-line tags. The new `setparams` step and oracle detect that error.
- `bash tools/bench/cross-editor/run.sh 1 /tmp/omamovie-rc-final cuts` passed for
  OmaMovie/MLT × VA-API/libx264: each output had 256/256 decoded frames, 10.677 s duration,
  all 256 picture frames matched the five reordered source intervals (worst luma PSNR
  42.04–45.09 dB), and audio at 15 points matched the correct source intervals (minimum
  normalized correlation 0.915, measured lag 0 ms, minimum relative level 0.68).
  [Full report](2026-10-17-rc-cuts.txt).
- `bash tools/bench/cross-editor/run.sh 1 /tmp/omamovie-rc-audio-v3 whole` passed for
  all four routes: 479/479 frames, 19.978 s duration, worst luma PSNR 41.08–45.03 dB,
  minimum audio correlation 0.988, measured lag 0 ms, and minimum relative level 0.70.
  [Full report](2026-10-17-rc-whole.txt).
- `bash tools/bench/long-export/run.sh 25 /tmp/omamovie-rc-long` passed: 45,000 decoded
  output frames, video 1,500.000 s, audio 1,500.011 s; 15 selected picture checks
  passed across reordered cuts and looks, with the dissolve between its sources. Audio
  at beginning/middle/end was non-silent. Export took 344.75 s and wrote 1.61 GB.
  [Full report](2026-10-17-rc-long-25min.txt).

The initial picture/frame-count oracle was checked against the source itself (pass) and
against a 479-frame output presented as the 256-frame cuts scene (failure). After audio
matching and color checks were added, the runner returned nonzero when the interim
fixture lacked color tags and retained the failed output for diagnosis. The final
oracle then passed the four-route cuts and whole-source runs above.
As a focused negative check, the v3 picture stream was copied unchanged and its audio
replaced with the older sine-tone fixture. `assert_output.py whole` failed at 0.999 s
with correlation 0.054, so correct picture and duration alone cannot satisfy the
audio oracle.

## Remaining RC gate

Run `scripts/rc-validation.sh` on the frozen SHA with its default five alternated rounds
per scene and 120-minute long export; retain its report and fixture hash. The pilot's
25-minute export validates the new long-form checker but does not replace the full
two-hour gate. The audio oracle checks 15 cut interior points and three whole-source
points with a 50 ms alignment search; it does not continuously compare every audio
sample or exercise Kdenlive's interactive editing workflow.
