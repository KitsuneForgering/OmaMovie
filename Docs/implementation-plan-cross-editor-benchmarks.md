# Cross-editor benchmarks and workflow tests (planned, 2026-10-14)

Purpose: compare complete editing tasks on declared media, hardware and output, from
simple editors through professional suites. The existing
[`tools/bench/filmcraft-equivalent/run.sh`](../tools/bench/filmcraft-equivalent/run.sh)
measures OmaMovie on FilmCraft's fixture recipes; the [2026-10-14
analysis](analysis/omamovie-2026-10-14-filmcraft-benchmarks.md) compares published
FilmCraft figures from other machines and methods. It is context, not a direct result.
No named competitor has been run under the protocol below.

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
- [ ] Add deterministic OmaMovie tests for the shared scene's edit, save/reopen and
  independently decoded export. Retain long-form and lifetime stress from
  [M10](implementation-plan-m10-validation.md); benchmark runs are evidence, not CI
  pass/fail tests against another product.
- [ ] Store raw results, commands, versions, fixture hashes and deviations under
  `Docs/Research/evidence/`; publish a concise comparison with uncertainty, output
  correctness and explicit `not comparable` cells. Recheck candidate support when the
  experiment actually runs.

Done when another person can reproduce at least the shared native Linux cases from
the recorded inputs and steps, independent output checks pass, and each comparative
claim identifies its machine, editor version, settings and measurement method.

[← Implementation plan](implementation-plan.md)
