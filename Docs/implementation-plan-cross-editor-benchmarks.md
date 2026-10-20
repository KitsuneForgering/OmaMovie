# Cross-editor benchmarks and workflow tests (planned, 2026-10-14)

Purpose: compare complete editing tasks on declared media, hardware and output, from
simple editors through professional suites. The existing
[`tools/bench/filmcraft-equivalent/run.sh`](../tools/bench/filmcraft-equivalent/run.sh)
measures OmaMovie on FilmCraft's fixture recipes; the [2026-10-14
analysis](analysis/omamovie-2026-10-14-filmcraft-benchmarks.md) compares published
FilmCraft figures from other machines and methods. It is context, not a direct result.
No named competitor has been run under the protocol below.

The pre-release correctness gate is `scripts/rc-validation.sh [report-dir] [long-minutes=120]
[comparison-repeats=5]`. It runs the release tests, 3,000 seeds × 200 stateful edit steps,
shared cuts and full-source exports
through OmaMovie and MLT's `melt` on both encode routes, then a two-hour edited OmaMovie
export. `assert_output.py` now fails on a wrong frame count, dimensions, duration,
incomplete per-frame comparison or a worst luma PSNR below 35 dB. A seeded-noise audio
fixture lets it match each cut's source interval at three points and detect silence,
wrong audio or drift beyond its 50 ms encoder-delay search. A failing comparator output is
reported, never interpreted as an OmaMovie speed win. The long-form oracle checks frame
count, A/V duration and sound at beginning/middle/end in addition to its cut, look and
dissolve checks. Record the exact SHA, hardware, versions, load and complete reports for
the RC; a passing benchmark on a different machine cannot substitute for that run.
The [2026-10-17 pilot](Research/evidence/2026-10-17-rc-correctness-pilot.md) passed one
round on each shared scene, the extended edit properties and a 25-minute long edit;
the default five-round/two-hour gate remains to be run on the frozen RC SHA.

## Comparator set and eligibility

| Tier | Candidates | Comparison boundary |
|---|---|---|
| Simple | [OpenShot](https://www.openshot.org/files/user-guide/installation.html), [Clipchamp](https://support.microsoft.com/en-us/clipchamp/what-clipchamp-products-are-there-and-what-s-their-cost) | OpenShot has a Linux build: same-machine performance and workflow candidate. Clipchamp personal runs in Edge/Chrome on Linux: workflow and perceived latency candidate; record browser, network and cloud-processing effects separately. |
| Intermediate | [Shotcut](https://shotcut.org/download/), [Kdenlive](https://docs.kdenlive.org/en/getting_started/installation.html) | Linux builds: primary same-machine comparators for shared native workflows. |
| Professional | [DaVinci Resolve](https://www.blackmagicdesign.com/event/davinciresolvedownload), [Adobe Premiere](https://helpx.adobe.com/premiere/desktop/get-started/technical-requirements/adobe-premiere-pro-technical-requirements.html), [Final Cut Pro](https://support.apple.com/en-ca/111903) | Resolve offers Linux, subject to an actual supported codec, edition, driver and install check on the test host. Premiere requires Windows/macOS and Final Cut Pro requires macOS: use them for workflow and output-quality reference until equivalent hardware and an independently documented cross-OS protocol exist. Do not mix their speed figures into a Linux ranking. |

Verify each candidate's current version, availability, license/edition, codec support
and export settings before a run. If a workflow or source codec is unsupported, record
that explicitly and use a separate compatible-media case; never silently transcode a
source for one editor. A famous name alone does not make a useful comparator.

## Shared workload and correctness contract

1. Publish versioned, redistributable fixture recipes and hashes: short 1080p30
   H.264/AAC, 1080p60 Omarchy-style capture, 4K long-GOP H.264/HEVC, and a mixed
   frame-rate/audio case. Include a 20-minute source and synthetic 1,000/5,000-clip
   projects only where the editor can represent the same edit. Record color metadata,
   time bases, GOP length, source duration and storage location.
2. Define tasks by visible result: import and organize; trim/split/ripple and undo;
   seek and scrub; play a one-track and a three-layer picture-in-picture scene with
   sound; save/reopen; export a short and a long edit. Add captions, color, speed and
   recovery as separate feature-specific cases when semantics match. Record setup
   steps and mismatched features instead of substituting an easier operation.
3. Pin equivalent export intent before timing: resolution, frame rate, color transfer,
   audio layout and quality target. Record actual codec, hardware/software encode,
   bitrate and file size. When identical settings are unavailable, report separate
   profiles rather than treating faster, lower-quality output as a win.
4. Independently decode outputs with FFmpeg/ffprobe and check frame count/duration,
   selected cut and overlay frames, audio continuity and A/V alignment; use image/audio
   tolerances justified by codec and color path. A render with wrong or missing content
   is a correctness failure, not a fast benchmark.

## Measurement protocol

- Same-machine runs: pin OS/session, CPU/GPU, power mode, driver, editor build/edition,
  display resolution, media disk, decode/encode route, proxy/preview policy and cache
  state. Measure cold and warm separately. Close the other editors and background
  workloads; randomize editor order and repeat each case at least five times. Keep
  raw logs, screen recordings and a replayable step sheet (automation where stable,
  human timing where UI automation would change behavior). For human runs, train the
  operator on each editor's task path first and alternate the order; report setup and
  learning time separately from timed editing.
- Report task completion time and operator actions for workflows; p50/p95/worst input-to-
  visible-frame and seek latency, missed playback deadlines, A/V error, export wall
  time/real-time factor, process CPU, peak RSS/VRAM and disk/cache growth for performance.
  Define start/stop events and the denominator for every frame metric. Use confidence
  intervals or the full small-sample spread; keep failed runs visible.
- Compare only shared tasks on the same host and equivalent outputs. Browser/network
  cases, different operating systems, unsupported codecs and vendor-specific hardware
  acceleration are separate tables. Neither published marketing figures nor the
  existing FilmCraft cross-machine numbers count as a head-to-head result.
- Keep a same-machine OmaMovie commit baseline before changing code. Rerun the exact
  fixture and route after changes; investigate a regression before attributing any
  change to RAII, algorithms or the competitor.

## Implementation and acceptance

- [ ] Extend `tools/bench` with a manifest for fixture hashes, scenario semantics,
  hardware/software paths and result schema; retain the existing FilmCraft-equivalent
  runner. Add a three-layer OmaMovie audit and instrument input-to-visible-frame,
  dropped/missed frames and end-to-end export verification where current audits lack
  them. Keep instrumented paths equivalent to normal editing.
  *Review (2026-10-19)*: the runners already print fixture sha256, scene semantics, editor,
  FFmpeg and VA-API versions, hardware path, alternated order and load per report, so a
  separate manifest file would duplicate them and is not added. The missing field was the
  CPU frequency policy, which moved one benchmark's medians by up to 1.9×: every
  `tools/bench/*/run.sh` now prints it through `tools/bench/host-power.sh` (driver,
  governor, EPP, power profile, AC). A machine-readable result schema stays open until two
  reports actually need automated comparison.
- [ ] Pilot one simple and one intermediate Linux editor, then Kdenlive/Shotcut and a
  professional Linux candidate if it runs the shared case on the reference host. Run
  the task sheet once to find semantic mismatches before gathering timed repetitions.
  *First pilot (2026-10-14, [evidence](Research/evidence/2026-10-14-cross-editor-export.txt))*:
  `tools/bench/cross-editor/run.sh` exports one BT.709-tagged 1080p source with OmaMovie and
  MLT's `melt` (Kdenlive 26.08's engine; Shotcut is not installed here), hardware and
  software H.264, five alternated rounds, FFmpeg checks only. Medians: VA-API 9.9 s vs
  6.5 s, x264 20.1 s vs 17.7 s (whole process, under load 6-8); melt adds one frame
  (480/479). The run found an OmaMovie deadlock (now fixed, code review part 4). An
  untagged source made both editors read BT.709 where FFmpeg wrote BT.601: fixtures must
  carry color tags. *Cuts scene (2026-10-14, [evidence](Research/evidence/2026-10-14-cross-editor-cuts.txt))*:
  `run.sh 5 DIR cuts` gives both editors five reordered cuts (OTIO via `oma-project convert`
  / a melt playlist) and checks all 256 frames against the source: both exact; medians
  VA-API 6.7 s vs 4.8 s, x264 12.4 s vs 10.5 s. Playback and seek are not shared yet.
  *After the export work (2026-10-15, [evidence](Research/evidence/2026-10-15-cross-editor-after-export-work.txt))*:
  VA-API now matches melt within noise (whole 6.0 vs 6.0 s, cuts 4.9 vs 4.8 s); x264 stays
  12-20% slower (19.7 vs 16.5 s, 11.7 vs 10.5 s). Load 8-11 for most of the run.
  *Shared project (2026-10-15, [evidence](Research/evidence/2026-10-15-mlt-roundtrip.txt))*:
  `roundtrip.sh` has melt write an MLT document (two cuts, a luma mix), render it, and OmaMovie
  import and export the same document: 132/132 frames, cuts frame for frame (worst 39.7 dB
  outside the dissolve, which OmaMovie centres on the cut).
  *Three layers (2026-10-20, [evidence](Research/evidence/2026-10-20-cross-editor-layers.txt))*:
  `layers.sh` stacks three distinct 1080p23.976 BT.709 clips, two scaled by half into
  opposite quadrants. MLT compositing is not imported by OmaMovie, so each editor gets its
  native form (an OmaMovie project from `layers.py`; melt `affine` transitions) and both are
  compared with FFmpeg's own scale+overlay of the sources. Three alternated rounds, all 240
  frames, PSNR identical per editor across rounds: OmaMovie 41.9 dB (VA-API) / 41.4 dB
  (x264), melt 44.2 / 44.5 dB. Median wall time 4.2 vs 15.5 s on the hardware route and
  4.9 vs 17.7 s on the software route; melt used 40 s of CPU where OmaMovie used 5.6 s.
  melt's `affine` composites on the CPU and Kdenlive's `qtblend`/GPU (movit) paths were not
  tried, and OmaMovie also decoded in hardware (ADR-0022), so this is not a ranking of the
  editors' best routes. The 3 dB gap is consistent with OmaMovie scaling in linear light
  (ADR-0006).
- [ ] Add deterministic OmaMovie tests for the shared scene's edit, save/reopen and
  independently decoded export. Retain long-form and lifetime stress from
  [M10](implementation-plan-m10-validation.md); benchmark runs are evidence, not CI
  pass/fail tests against another product.
  *Cuts scene in `app_tests` (2026-10-19)*: `tests/app/test_shared_cuts.cpp` imports the
  bench's five-reordered-cuts shape at fixture scale (24-frame 23.976 fps source) from
  `cuts.otio` and from `cuts.mlt` as melt writes it (melt renders that document as 19 frames),
  saves, reopens in a fresh Session, exports in software and matches all 19 frames to their
  expected source frames by an ffmpeg decode. Reordering two expected cuts makes both cases
  fail. A third case imports `melt_mix_23976.mlt`, written by melt 7.42 itself
  (`roundtrip.sh` at fixture scale): 22 frames, and outside both dissolve windows (melt
  10–13, OmaMovie centred 12–15) output 0–9 and 16–21 must be source 0–9 and 18–23; an
  off-by-one expectation fails it. The dissolve frames themselves are not checked.
  `cuts_steps.otio`/`.mlt` cut the 30 fps steps clip the same way and also check each output
  frame's sound by its loudness step over a whole-cycle window (11 cycles of 440 Hz);
  a one-frame sound shift fails both cases with the picture check disabled. melt renders
  the same MLT document with frame-exact sound at a constant −3.0 dB (0.705: its equal-power
  mono → stereo upmix), where OmaMovie copies mono to both channels at unity; loudness
  comparisons must account for that. A sixth case exports `cuts_steps.mlt` on the hardware
  route and requires the reported encoder to be VA-API or the libx264 fallback with its
  reason; on the reference Intel host it ran `VA-API H.264 (GPU)`, and with
  `LIBVA_DRIVER_NAME=oma_bogus` the fallback, both with every frame and sound step right.
  CI without VA-API covers only the fallback branch. On that untagged 320×180 source OmaMovie's RGB differs
  from melt's by design, not as a defect: ADR-0006 reads untagged SD as SMPTE 170M primaries
  and converts them to BT.709 (pure green → (73, 250, 0), matching the matrix), while melt
  keeps the samples. Pixel-level comparisons need tagged BT.709 fixtures.
- [ ] Store raw results, commands, versions, fixture hashes and deviations under
  `Docs/Research/evidence/`; publish a concise comparison with uncertainty, output
  correctness and explicit `not comparable` cells. Recheck candidate support when the
  experiment actually runs.

Done when another person can reproduce at least the shared native Linux cases from
the recorded inputs and steps, independent output checks pass, and each comparative
claim identifies its machine, editor version, settings and measurement method.

[← Implementation plan](implementation-plan.md)
