# Implementation plan — OmaMovie

> Created on 2026-10-02; evidence reviewed on 2026-10-03 and new research added on 2026-10-04 with skeptical-research. Engineering rules and invariants live in
> `CLAUDE.md`; this document defines **what to build, in which order and how to know it is
> done**.
>
> Update this file when a milestone is finished (tick the items, record deviations).
> Structural divergences need an ADR (`CLAUDE.md` §20).

---

## 1. Goal

Build a native local editor for simple recording/phone-video workflows on Omarchy, with
an extensible pipeline. Offline/no-account behavior, a contextual UI and GPU-first processing
are project choices. Real-time playback and support breadth are claims earned per tested
workload, hardware and driver. Professional parity and replacing the default editor are
long-term aspirations, not findings established by this research.

Research basis after the [skeptical review](Research/skeptical-review.md):

| Premise | Evidence / status | Effect on the plan |
|---|---|---|
| Consumer-media accessibility matters | Bounded Resolve 20 Linux codec rows; audience hypothesis | Keep phone/recording fixtures; compare current Kdenlive/Shotcut baselines |
| Recording integration | Installed scripts, version-sensitive directory/codec/audio behavior | Respect directory overrides and no-audio files; probe actual output |
| Recoverable edits and projects | Explicit lifetime/undo and Linux durability contracts | Atomic replacement plus file/directory sync, autosave and parser limits |
| Long-form editing | Duration, clip count, codec/GOP, effects and export stress different paths; current source inspection, no long-form benchmark | Preserve JSON/simdjson pending measured need; use the workload gate in [long-form research](Research/long-form-editing.md) before indexes, proxies or a new project store |
| Contextual/magnetic/preset UI | Documented feature examples; effectiveness untested | Preserve design choices; M6 baseline/task pilot before claiming usability |
| Final Cut connected editing | Apple documents primary/connected clips and a precise Position tool; OmaMovie ripple is track-local | Specify anchors and a non-ripple placement path before freezing the native project DTO; test against manual grouping ([research](Research/final-cut-pro.md)) |
| Agent-assisted editing with MCP | MCP defines external-host tools/resources, not a model or editing quality; OmaMovie has validated commands but no durable project yet | Keep a conditional local read/propose pilot after M7, compare against GUI/CLI, and require an ADR before implementation ([research](Research/agentic-mcp.md)) |
| Shared Vulkan device | ADR-0004 headless queues; S4/ADR-0005 validated Qt zero-flag queues for a static preview | M4 must pause live producers across Qt swapchain changes |
| Color | Current SDR subset; independent oracle and display stages pending | S6/ADR-0006; no HDR claim from RGBA16F storage |
| NVIDIA | Packaged FFmpeg runtime candidate; no hardware validation | S7; no universal speed ranking or toolkit-free build claim |
| OpenCL excluded | Existing maintainer scope decision; generic interop exists | Keep exclusion; no unsupported impossibility rationale |

Milestone checkboxes record implemented deliverables. Validation requires separate evidence.
S1/S2 and M0/M3 numbers below are prior reports, not rerun by the documentation audit.
New acceptance criteria are proposed pilot criteria set before future measurements; changes
to them must be recorded, rather than choosing thresholds after seeing results.

---

## 2. Milestone overview

M0–M9 are construction milestones toward v0.1, not separate releases.

```
M0 Foundation ─┬─────────────────────────────────────────────┐
               │                                             │
M1 Hardware spikes (S1–S6, in parallel with the end of M0)    │
               │                                             │
M2 libs/gpu + libs/media ── M3 Compositor ── M4 Playback/audio│
                                    │                        │
M5 Timeline model (can start after M0, no GPU) ──────────────┘
                                    │
                     M6 UI ── M7 Project + export ── M8 Creator ── M9 Interop + color ──► v0.1
```

M5 depends only on `libs/base` and can progress in parallel with M2–M4.

---

## 3. Milestones

Legend: **Deliverables** = what exists at the end. **Done when** = a verifiable criterion.
**Research** = where the decision came from.

### M0 — Foundation ✅ (2026-10-02)

**Deliverables**
- [x] Build/development dependencies declared by the PKGBUILD; `make deps` reads it.
- [x] Non-recursive `Makefile` (GNU Make, ADR-0001): `BUILD=debug|release|asan|tsan`, one `module.mk` per lib and per test suite.
- [x] Full warning set with `-Werror` on the libs; `.clang-format`, `.clang-tidy` (warnings are errors), `.editorconfig`, `.gitignore`.
- [x] **Dependency graph from §5.2 enforced by Make**: a forbidden dependency or an unregistered lib fails before compiling. The graph now includes `base`, `gpu`, `media`, `compositor`, `audio` and `timeline`.
- [x] `libs/base`:
  - [x] `Rational`, `RationalTime`, `TimeRange`, `FrameRate`, `SampleRate`; 128-bit `rescale` with overflow detection and explicit rounding; exact comparison across timebases.
  - [x] `oma::Error` + `Result<T>` (`std::expected`).
  - [x] Logging facade with the §19 categories, no Qt.
  - [x] Job system: thread pool, cooperative cancellation, progress, ordered shutdown.
- [x] Tests with **Cest** (vendored in `third_party/cest`, pinned to a commit), `make test`, filter and JUnit.
- [x] CI (GitHub Actions, Arch container): g++ and clang++ × debug/asan/tsan/release, plus format and clang-tidy; Conventional Commits check; tag-based release; Dependabot.
- [x] Fixture generator (`tests/fixtures/generate.sh`): H.264 CFR/NTSC/VFR, 10-bit HEVC, AV1+Opus, rotation, truncated file, 44.1 kHz WAV, PNG.
- [x] ADR-0001 (Make, C++23, Cest, conventions, license), ADR-0002 (time and `TimeMap` for speed ramps), ADR-0003 (threading and job system).

**Done when**: `make test` passes locally and in CI; time tests cover 23.976/29.97/59.94,
44.1/48/96 kHz, hours in a 90 kHz timebase, rounding and overflow; `CLAUDE.md` §3 reflects
the real commands.

**Result**: 45 tests (109 assertions) pass in `debug`, `release`, `asan` and `tsan` with
GCC 16 and Clang 22, locally and in CI; `make tidy` and `make format-check` are clean. TSan
found a lock-order inversion in the logging test, fixed and documented in the sink contract.
The first CI run caught a const-correctness warning from the container's newer clang-tidy.

**Research**: `CLAUDE.md` §6, [capcut](Research/capcut.md).

---

### M1 — Hardware spikes

Code in `tools/spikes/`, disposable; whatever proves its value moves to the libs in M2/M3.
Results in `Docs/spikes/Sn-<name>.md` with numbers, driver versions and a conclusion.

| Spike | Question | Done when | Feeds |
|---|---|---|---|
| **S1** Inventory ✅ | Which codecs/profiles does the Iris Xe decode and encode through Vulkan Video, VA-API and QSV? | Codec × API × bit depth table — **done**, see `Docs/spikes/S1-hardware-inventory.md`: VA-API is the default decode path; Vulkan Video is behind `ANV_DEBUG` on TigerLake; Vulkan compute codecs (ProRes, FFv1) work without flags | ADR-0004 |
| **S2** FFmpeg on OmaMovie's device ✅ | Does the installed FFmpeg accept a `VkDevice` created with Vulkan-Hpp and deliver `AVVkFrame` without readback? How does queue locking work? | H.264/HEVC/AV1 decoded into `AVVkFrame`, no explicit host transfer reported in the application mapping path — **prototype done; follow-up validation pending**, see `Docs/spikes/S2-ffmpeg-own-device.md`: VA-API → DMA-BUF → Vulkan on our device, exact luma, no `AV_HWFRAME_MAP_DIRECT`; queues via `VK_KHR_internally_synchronized_queues` | ADR-0004 |
| **S3** Decode/encode paths ✅ | VA-API vs. QSV (vs. flagged Vulkan Video, for reference) per codec on the Iris Xe; FFv1 vs. ProRes for proxies | Frame time, CPU and power per codec; written selection policy — **decode done** (2026-10-04), see `Docs/spikes/S3-decode-paths.md`: QSV adds nothing over VA-API (keep ADR-0004 order); hardware decode saves 5–6 cores at 1080p60, not throughput; ProRes (intra, CPU-encoded: `prores_ks_vulkan` output is invalid at 1080p) is the proxy candidate, FFv1 Vulkan decode runs at 19 fps. Power not measured (RAPL root-only); export encode paths moved to M7 | ADR-0004 |
| **S4** Qt Quick on the same device ✅ | Can Qt Quick present a compositor image on the same device without a render-path readback? | The repeatable offscreen diagnostic checks identical physical/logical device and queue, borrowed compositor-image pixels and 60 frames with no validation errors. A prior static visible experiment passed bounded queue contention. Its test-pattern window was replaced after user feedback: `make run-gui` now opens real videos with a temporary software-frame upload. Production on-screen GPU handoff belongs to M4. See [S4](spikes/S4-qt-shared-device.md) | ADR-0005 |
| **S5** Minimal compositing ✅ | 2 videos + 1 image with transform/crop/opacity and YUV→RGB | 1080p60 frame time measured and recorded — **done** (2026-10-04), see `Docs/spikes/S5-minimal-compositing.md`: needed RGB image support (planar GBR from the decoder); sequential decode + composite p50 8.2–8.3 ms with VA-API (3–4 of 600 frames over 16.67 ms), 9.1–9.3 ms software with a tail to 22 ms; no presentation | M3 |
| **S6** libplacebo | Does it operate on OmaMovie's images/device without copies? Quality and cost? | Decision "use libplacebo for color/scaling/LUT or our own shaders" | ADR-0006 |

S7 (NVIDIA path) and S8 (AMD) need other hardware; they are scheduled for when a machine is available (§6).

**Done when**: ADR-0004 evidence limits are recorded, and ADR-0005/0006 are written after S4/S6 pass.
See [validation protocols](Research/skeptical-review.md#validation-protocols); S1/S2 alone do not complete M1.

**Research**: [hardware-strategy](Research/hardware-strategy.md), [vulkan](Research/vulkan.md).

---

### M2 — `libs/gpu` and `libs/media`

**`libs/gpu`**
- [x] `VkInstance`/`VkDevice` creation with Vulkan-Hpp (`vk::raii`, `VULKAN_HPP_NO_EXCEPTIONS` + `VULKAN_HPP_RAII_NO_EXCEPTIONS` → `std::expected`), include confined to `.cpp` files + PCH.
- [x] Extension list: interop (DMA-BUF, DRM modifiers, external memory/semaphore fd), video (decode/encode), those Qt requires.
- [x] Queue synchronization for current FFmpeg/compositor consumers.
- [x] Qt queue retrieval and stable-frame submission on zero-flag queues verified in S4 with direct render-thread queue-lock signals.
- [ ] M4: pause/drain live GPU workers before Qt swapchain create/resize/teardown, including cancellation and device loss; frame signals alone do not enclose Qt's `vkDeviceWaitIdle`. *For the viewer*: solved by construction — the compositor submits from Qt's render thread during sync (the thread that also recreates the swapchain), with a recursive queue lock. Still open for any producer on another thread (hardware decode, export) and for device loss.
- [ ] Evaluate VMA against the existing `gpu` allocation wrappers; retain explicit imported-memory lifetime.
- [x] GPU command/resource wrappers exist.
- [ ] Ordered destruction and cancellation verified under concurrent consumers (consumers → pools → device).
- [ ] Capability table + device selection (hybrid laptops: identity matching and power behavior measured before claiming power-safe discovery).

**`libs/media`**
- [x] Probe → our own structure (streams, codec, timebase, duration, color, rotation, audio layout).
- [x] Demux, software decode (correctness reference).
- [x] Hardware decode with the per-codec/driver policy (ADR-0004: mapped VA-API, Vulkan Video), logged software fallback.
- [ ] NVDEC on NVIDIA, once NVIDIA hardware is in the matrix.
- [x] `Frame` wrapping `AVVkFrame` (timeline semaphore contract).
- [ ] Frame pool, no per-frame allocation in steady state.
- [x] Accurate seek by PTS (VFR), `AV_NOPTS_VALUE` converted to `std::optional`.
- [x] Audio decode + resampling to planar float32.
- [ ] Compute-shader intermediates (FFv1/ProRes) verified for the actual build; compare storage/seek/quality before choosing proxy format.

**Tests**: CFR/VFR, 8/10-bit, H.264/HEVC/AV1, rotation, AAC/Opus and truncated fixtures;
**a real Omarchy recording** (generated locally); a fuzzing target for the probe.

**Done when**: real-time 1080p60 H.264/HEVC/AV1 decode on the Iris Xe **without readback**,
measured end-to-end with the audit protocol (`oma-bench` is still a target tool); seek tests pass on VFR. Record actual frame format, copies, repetitions, drops and memory bounds.

**Research**: [vulkan](Research/vulkan.md), [cuda](Research/cuda.md), [omarchy-integration](Research/omarchy-integration.md).

---

### M3 — Compositor and render graph

- [x] Render graph as plain data (testable without a GPU).
- [x] Vulkan compositor: YUV→RGB with an SDR metadata subset; transform, crop, opacity, basic blend modes; layer stack.
- [ ] Independent range/primaries/transfer/chroma validation; explicit rejection/degradation for unsupported PQ/HLG and a preview display transform. *Partial*: `VulkanCompositor::encode_display` applies the sRGB transfer function to the linear output (RGBA8 for an SDR display), tested against the formula within one 8-bit level; the full SDR/HDR policy is still ADR-0006's.
- [x] Working space per the ADR-0006 proposal (linear BT.709, premultiplied, RGBA16F output for the preview).
- [ ] If S6 approves: libplacebo as a color/scaling stage.
- [x] GLSL shaders → SPIR-V at build time (glslc, embedded).
- [ ] Persisted pipeline cache (`$XDG_CACHE_HOME/omamovie`).
- [x] CPU reference compositor (`CpuCompositor`), compared with the GPU output in tests.
- [ ] `ComputeBackend` interface for effects (`VulkanComputeBackend` + `CpuBackend`), with the first effect; non-real-time sections are reported. *Partial (2026-10-03)*: the first effects run inside both compositors instead, tested GPU against CPU: per-layer color adjustments and filter looks (`src/look.hpp`) and blur/sharpen as a reduce pass plus two separable gaussian passes into reduced RGBA16F images. The interface waits for an effect that does not fit the layer pass.
- [ ] Edge anti-aliasing for rotated layers; chroma siting from the stream (center-sited for now).

**Done when**: the S5 scenario runs on library code; GPU × CPU comparison within tolerance, independent SDR vectors and the declared supported-color contract pass. Record repeated frame times, copies and memory use; end-to-end real-time claims also require M4/S4 presentation measurements.

**Measured (2026-10-03, Iris Xe, `tests/compositor`)**: GPU vs CPU reference within 0.002
(linear, half-float output) on uploaded, VA-API and Vulkan Video frames. Three 1080p layers,
submit to completion: 3.3 ms with VA-API frames, 2.9 ms with Vulkan Video frames, 3.7 ms with
uploaded software frames — isolated means below the 16.7 ms interval. These are prior timing samples, not end-to-end playback evidence. The current test warms once and averages 20 renders; it does not record tail latency, new-frame decode or presentation. CPU/GPU agreement is not an independent color oracle.

**Research**: `CLAUDE.md` §9, [premiere-pro](Research/premiere-pro.md), [davinci-resolve](Research/davinci-resolve.md).

---

### M4 — Playback and audio

- [x] PipeWire output; real-time callback without allocation or contended locks (lock-free SPSC `SampleRing`; a null output paced by a steady clock for CI and machines without PipeWire). Tested in `tests/audio`, including a producer/consumer ordering test under TSan; TSan does not prove RT deadline safety.
- [x] Master clock = audio; video drops/repeats frames. `PlaybackClock` (audible position from consumed frames minus device latency, re-anchored on seek) drives the editor's viewer: `AudioPlayer` (apps/omamovie) renders the timeline's audio on a dedicated pipeline thread into the output ring, and the video frame shown is the one at the audible sample. Silent fallback: the null output keeps an audio-paced clock. *Not measured yet*: the 10-minute drift protocol below.
- [x] Timeline audio mix: gain, linear fades and mute per clip, sample-accurate clip boundaries, any number of overlapping clips (`oma::audio::mix_planar`, tested); clip audio at speeds other than 1 stays silent until time-stretching exists. The mix lives in `libs/playback` (`TimelineAudio`, tested against fixtures): a clip continuing the same media where the previous one stopped (a split) keeps its decoder; per-clip three-band equalizer (`oma::audio::Equalizer`) and noise reduction (FFmpeg `afftdn` in `AudioDecoder`, noise level measured from the clip). Mono sources play as dual mono at full level.
- [ ] Playback scheduler: decode ahead with bounded queues, cancellation on seek. *Partial*: audio renders 0.25 s ahead into the bounded ring and restarts on seek/edit; video decodes up to 8 frames ahead in a bounded queue (`VideoScheduler`). Open: the UI thread waits for both producers and reopens every decoder on each play/seek (audit 2026-10-03).
- [ ] J/K/L (multiple speeds), frame step. *Partial*: frame step (←/→, Shift for 10) lands on exact rational frame times; J/L shuttle at 1×, 2× and 4× in both directions. Reverse (2026-10-04): stepping back past the decoded frames seeks one chunk earlier (up to 64 frames, half of a 384 MB history budget) and decodes forward to the target, keeping the chunk, so each group of pictures is decoded about once; a 1080p H.264 file with 250-frame GOPs went from 139-150 to 3.3-4.9 ms per frame played backwards (scratch benchmark through `FrameSource`, release, Iris Xe, software decode). Forward playback keeps only 8 frames of history. Open: the history does not survive restarting playback (each start opens new decoders).
- [x] On-screen compositor-image handoff for the viewer (2026-10-03): the timeline's `Composition` becomes a `RenderGraph` with every visible layer and its fit/crop/transform/opacity/blend; `PreviewItem` composites and encodes on Qt's render thread and Qt samples the compositor's own `VkImage` (no readback, no CPU conversion). The default decode remains software (one plane upload per frame, CLAUDE.md §7.3). The smoke check verifies that the viewer shows composited video; a run under the Vulkan validation layer with synchronization validation reports no errors when no window capture is taken (Qt's `grabWindow` itself triggers swapchain hazards).
- [ ] Hardware decode pilot in the viewer. *Partial (2026-10-04)*: `OMA_PREVIEW_HARDWARE=1` lets the render thread decode with the Qt/compositor `gpu::Device`, trying VA-API → Vulkan then Vulkan Video where supported, with software fallback; thumbnails stay on the CPU path. Doing decode on the render thread avoids concurrent submissions during Qt's swapchain teardown, but also blocks presentation while decoding and bypasses `VideoScheduler`'s lookahead. On Iris Xe, smoke checks negotiated VA-API for H.264 and HEVC. An 8-second 1080p60 H.264 audit produced 372 composites and 20.67 ms p99 A/V error with hardware decode; the software route produced 371 composites and 20.00 ms p99 A/V error, with 105 scheduler drops. Both failed the 16.67 ms p99 target. Hardware-route dropped frames are not measured by the scheduler, so the audit reports them as unmeasured. Keep hardware decode opt-in until full-scene measurements show a benefit and the gate below passes. Open: bounded decode ahead with safe admission around Qt swapchain changes, per-stage timing, longer codec/4K/seek tests, and VA-API render-node selection on hybrid systems.
- [x] Qt Quick editor shell through `make run-gui`: Projects/Edit, media library, viewer, responsive timeline, play/pause and seek. The storyline is the M5 `Editor` (commands and undo/redo through `Session`); the viewer evaluates the timeline at the playhead, decodes forward without reseeking during playback and follows the audio clock. Decode-ahead queues remain pending.

**Done when**: A/V drift measured over 10 minutes against the proposed predeclared pilot bound in the audit; frame presentation and device latency are accounted for, with no growing drift. Seeks, speed changes, underruns and device changes recover; UI stays responsive; TSan clean. TSan does not prove RT deadline safety.

**Hardware decode pilot gate (partially run)**: compare the current
software-decode/upload viewer with hardware decode on the same Intel machine,
files, timeline, output and effects. Include H.264/HEVC/AV1, 1080p and 4K,
one- and two-layer scenes, seeks and a 10-minute playback. After warm-up,
repeat runs and record negotiated path, decode/upload/composition/presentation
times (p50/p95/p99), dropped frames, CPU/GPU use and memory growth; check
image/color agreement and Vulkan synchronization validation. For a claimed
1080p60 scene, require p99 end-to-end frame time ≤ 16.67 ms and no sustained
drops, corruption, deadlock or unbounded memory growth. Keep the hardware path
as the default only for tested codec/driver combinations that improve the full
viewer without violating those checks; otherwise retain software fallback.
The isolated [S2](spikes/S2-ffmpeg-own-device.md) and
[compositor timings](Research/evidence/2026-10-04-gpu-effects.txt) do not
establish that uploads dominate playback: the latter include no decode or
presentation and show blur can exceed the frame budget. If composition
dominates, optimize that effect; if queue waits dominate, test bounded frames
in flight separately before changing synchronization.

**Research**: `CLAUDE.md` §12–13.

---

### M5 — Timeline model (in parallel with M2–M4)

- [x] Tracks (video, audio; caption type already in the model), clips by stable media ID.
- [x] Commands with a single history transaction: insert, overwrite, append, split, trim (start/end, with or without ripple), roll, slip, slide, move, delete, ripple delete (clip and range), speed, video/audio properties, tracks, markers; user transactions are all-or-nothing.
- [x] Undo/redo; invariant `validate()`, checked by `Editor::execute` after every command (a rejected edit leaves the timeline and history untouched).
- [x] Magnetism as a **UI policy** over generic commands (`Session`: storyline cuts for reorder/drop, `snapSpan` for lane moves and trims; the commands stay generic).
- [x] Markers in the model.
- [ ] Clip time mapping per ADR-0002 plus a complementary interpolation/domain ADR: constant speed first; freeze/reverse have explicit timeline duration, ramps integrate speed with explicit rounding. **Constant speed is implemented** (`TimeMap::constant`); freeze, reverse and ramps wait for the complementary ADR.
- [x] Per-instant evaluation for a time `t` (`oma::timeline::evaluate` → `Composition`), see the deviation below.
- [x] Commands with no UI dependency; command serialization only when a concrete persistence/scripting requirement is established (undo alone does not require it).
- [ ] Decide the connected-clip contract before ADR-0007 freezes the native DTO: an explicit anchor to a primary clip and sequence-time offset, or a documented simpler grouping policy. Specify split, trim, ripple move/delete, speed changes, locked tracks and orphan handling; make each edit one atomic undo entry. Test relative timing, invalid anchors, save/load and undo/redo. Adopt anchors only if the M6 task comparison justifies their complexity ([Final Cut research](Research/final-cut-pro.md)).

**Implemented (2026-10-03, `libs/timeline`, `tests/timeline`)**. Choices to know when reading the code:
- *Composition instead of RenderGraph*: `compositor::RenderGraph` lives in `libs/compositor`, which depends on `gpu`; §5.2 allows `timeline` only `base`. The timeline therefore describes an instant in its own terms (video layers and audio sources with media IDs and media times, bottom first) and the playback layer, which owns the decoded frames, builds the RenderGraph by resolving each media time to an input. The visual property types mirror the render graph's.
- *Sequence timebase*: every timeline position is an integer tick of one sequence timebase; `Timeline::default_timebase` uses 1/lcm(frame-rate numerator, sample rate) so frames and samples are both exact (29.97 fps at 48 kHz → 1/240000). Times off the grid are rejected, never rounded.
- *Exact source positions*: `source_in` keeps any timebase, and trims/slips move it by exact rational amounts; rounding to a media PTS (floor) happens only in `evaluate`. Trim forward and back returns the same source in on a 1/15360 media timebase.
- *Stored duration*: a clip stores its timeline duration (per the ADR-0002 review note, duration cannot be derived from source displacement alone once freeze exists); speed changes recompute it, rounding down to the grid to stay inside the source.
- *History*: primitives record exactly what they changed, so undo restores exact prior state; editing operations plan their primitives on first apply and redo re-applies the same plan. IDs only grow, even across undo. Ripple affects the edited track only.
- *Media and history*: importing registers media with `Editor::add_media`, outside the history (undoing an edit never unregisters media the library still shows); `Editor::clear_history` drops undo/redo after a new project's initial tracks (and, later, after loading).
- *Not yet*: magnetism/snapping policy, caption clips, keyframes, transitions, track-linked (sync-locked) ripple, a timeline-wide limit on history size, and the Qt model that exposes the timeline to QML (M6).

**Done when**: every operation is tested, including combinations and undo/redo of
transactions; render graph snapshots for known states.

**Research**: [premiere-pro](Research/premiere-pro.md), [imovie](Research/imovie.md), [final-cut-pro](Research/final-cut-pro.md), [davinci-resolve](Research/davinci-resolve.md).

---

### Audio editing ahead of order (2026-10-03)

The maintainer chose to finish audio editing and start video effects before M7
(`CLAUDE.md` §24 order), with persistence and export still required for v0.1.
Done: audio lanes (import sound, place, move between lanes, trim without ripple),
waveforms (`playback::compute_waveform`, 10 ms peaks and RMS, session-only until
ADR-0009 decides the cache), detach audio for J/L-cuts (`edit::detach_audio`,
`Clip::audio_detached`), Volume "More" (equalizer presets, noise reduction,
peak normalize). Not done: linking lane clips to the storyline clip they belong to
(sync-locked ripple), loudness (LUFS) normalization, speed-changed clip audio.
Prepared for more clip audio effects (2026-10-04): `audio::Effect` (in-place planar
processing without allocation, reset on seek) and `audio::EffectChain` (slots that keep their
state across reconfiguration); the equalizer is the first effect and playback builds each
clip's chain in one place (`libs/playback/src/clip_effects.cpp`). Noise reduction stays in
the decoder (FFmpeg `afftdn`).

### Video effects ahead of order (2026-10-03)

Done: Color (exposure, contrast, saturation, temperature), Crop and framing
(fit, edges, position, scale, rotation), Effects (six filter looks with previews,
soften/sharpen), transitions (ADR-0011), grading (ADR-0012, 2026-10-04: lift/gamma/gain
wheels stored as ASC CDL, monotone curves, tetrahedral 3D LUTs from `.cube` files with a fuzz
target). Not done: "Auto" color, Ken Burns (keyframes), on-viewer handles, the cut editor
(double click on a junction), scopes. Blur reworked (2026-10-04, research
`Docs/Research/gpu-effects.md`): the source is linearized and box-reduced once (across, then down), the gaussian
runs on the reduced image (RGBA16F) and the composite reads it back bilinearly; maximum blur
went from 52.6 to about 3 ms at 1080p and from 214 to 5.7 ms from a 2160p source, and its strength
no longer depends on the file's resolution (evidence §5). Ken Burns through transform keyframes (M8, partial).

**Next effect selection (proposal, 2026-10-04):** [effect research](Research/post-production-effects.md)
compares the editors already researched with OmaMovie's existing tools. Pilot a
region mask with opaque cover/local blur or highlight and a focus/zoom preset
that emits editable transform keys. Compare crop/manual keys and inspect every
frame of the decoded export before claiming a sensitive area is hidden. Audit
Vintage, Sepia, Cool and Warm against saved presets; remove a redundant look
from new-project discovery only with a tested migration for old projects.
Chroma key follows PiP/alpha handling. Stabilization, video denoise and automatic
tracking remain footage/performance pilots, not committed built-in effects.

### M6 — UI

Reference design: `Docs/ui-design.md`.
UI/UX tasks and acceptance criteria: [TDDO](ui-ux-tddo.md).

- [ ] Qt Quick shell: MediaPanel, PreviewPanel, TimelineView, Inspector, TransportControls. *2026-10-04*: no permanent adjustments bar (the clip's context menu opens the drawers and marks active adjustments); the library sidebar hides in any window width, leaving the viewer centered.
- [ ] MediaPanel with a **"Recordings"** source (respects `OMARCHY_SCREENRECORD_DIR`, then `$XDG_VIDEOS_DIR`/fallback; handles growing and no-audio files); thumbnails and waveforms through the job system + invalidatable cache.
- [ ] TimelineView: tracks, drag, trim, split, snapping, **mouse-centered** zoom, overview bar (minimap). *Partial*: storyline with ripple trim by edge drag, split, scrubbing playhead, minimap; clip dragging (storyline reorder, lane moves, library drops); snapping to clip edges/playhead with a toggle and mouse-centered Ctrl+wheel zoom (2026-10-04). Video tracks above the storyline remain.
- [ ] Measure timeline interaction with the [long-form workload set](Research/long-form-editing.md#proposed-gate-before-optimizations), separating duration from clip count. Profile `Session`'s full `QVariantList` rebuilds, repeated QML delegates, timeline snapshot copy and whole-model notifications. Pilot visible-range delegates or incremental updates only if they dominate the measured stalls; preserve drag, snapping and undo behavior.
- [ ] Expose deliberate precise placement using the existing overwrite/lift operations; decide whether a persistent Position mode is needed by testing replacement and gap-preserving tasks against direct commands. Show which clips move or are replaced before committing a drag, including keyboard access.
- [ ] Contextual inspector: video (transform, crop, opacity), audio (gain, fades), text (simple title); indicator of the adjustments active on a clip.
- [ ] Direct manipulation in the preview (position, scale, crop) emitting commands.
- [ ] Central action system (id, name, shortcut), independent of the input device; full keyboard editing. *Partial*: one QML registry of `OmaAction`s (name, default keys, handler) for every shell command; remapping and the command palette remain.
- [ ] Omarchy platform module (`apps/omamovie/src/platform/omarchy/`): theme through `omarchy-theme-color --all` + a watcher on `~/.local/state/omarchy/current/` + fallback; Omarchy font; app_id `omamovie`.
- [ ] `omamovie.desktop`, icon, documentation of the Hyprland opacity rule.
- [ ] Projects screen (recent projects + recent recordings with "Edit") and open-by-origin rules; single instance (`ui-design.md` §3.1).
- [ ] The Omarchy font across the UI, with live switching (`ui-design.md` §10.1).
- [ ] Initial set of our own SVG icons for v0.1 (`ui-design.md` §10.2).
- [ ] OmaMovie shortcut assignments inspired by the iMovie/Final Cut convention; check action semantics, focus and compositor conflicts (`ui-design.md` §8.1).

**Done when**: a short vlog can be edited from start to finish, including keyboard-only; the
UI event latency is measured against a predeclared interaction budget; drawer/magnetic behavior is compared with a conventional inspector/track baseline at wide/half/narrow sizes. Theme switching tolerates missing/partial files without touching the preview; Qt queue and color gates have passed.

**Research**: [imovie](Research/imovie.md), [final-cut-pro](Research/final-cut-pro.md), [movie-maker](Research/movie-maker.md), [other-editors](Research/other-editors.md), [omarchy-integration](Research/omarchy-integration.md).

---

### M7 — Project and export

- [ ] ADR-0007 (project format) and ADR-0009 (cache). *ADR-0007 accepted (2026-10-04)*; ADR-0009 pending.
- [ ] Versioned serialization (DTOs separate from the model), chained migrations, atomic replacement (same-filesystem temp, file sync, rename, parent-directory sync with errors surfaced), separate autosave, relink by fingerprint. *Partial (2026-10-04, `libs/project`)*: `project::Document` ↔ JSON (simdjson), `Timeline::restore` with full validation, atomic `save`, relative/absolute paths with fallback, fingerprints, newer versions refused, unknown top-level fields kept, fuzz target. App Save (`Ctrl+S`, first save asks where) and Open (`Ctrl+O`, Projects screen, or `omamovie file.omamovie`): saving runs on the job worker; opening re-imports the library under the saved IDs and restores the timeline; the title and Projects card show unsaved changes, and the discard guard uses them (UX-01/02). Open: autosave, relink UI, Save As, Save in the discard dialog, the first migration.
- [ ] Give project automation a stable persisted project identity/content revision and a conflict check for stale edits; keep the native DTO independent of MCP. The future agentic pilot cannot rely on `Editor::revision()` alone across reloads or processes.
- [ ] Test interruption recovery and mistaken-edit recovery separately. If autosave cannot restore the latter, add a user-restorable prior project version with retention and an explicit restore path; distinguish project data from source-media backup.
- [ ] SQLite media index, if justified in the ADR.
- [ ] Record cold/warm project open and save by phase (read/parse, DTO/restore, reimport, UI materialization), project size and peak memory at the long-form gate. Keep JSON/simdjson unless measured project-file cost remains material after fixing avoidable copies; a SQLite media index is a separate cache decision, not an automatic project-format migration.
- [ ] Export: render graph → encoder (Vulkan/VA-API on Intel/AMD, NVENC on NVIDIA, software fallback) + mux, as a background job with progress and a notification (D-Bus).
- [ ] For longer exports, define temporary-output and cancellation semantics, disk-full handling and an independently decoded two-hour output check at cuts, effects and beginning/middle/end A/V points. Investigate segmented/restartable export only after the continuous path is correct and interruption cost is measured.
- [ ] `oma-project inspect | validate | dump` for the native format.
- [ ] Packaging: `omastore.toml`, GitHub release with binaries, the app installed by `package()` (the `-git` PKGBUILD and dependency declaration already exist).

**Done when**: save/load round trip without differences; loader fuzzing and save-failure injection pass;
exported file checked with `ffprobe`, independent decode/content inspection and measured A/V sync; installation from the PKGBUILD on a
clean Omarchy machine.

---

### M8 — Creator

- [ ] Caption track, SRT/VTT import/export.
- [ ] Changeable canvas aspect ratio (ADR: normalized coordinates vs. pixels).
- [ ] Constant speed + speed ramps with named presets (curve from ADR-0002).
- [ ] Keyframes with interpolation on every animatable parameter; simple dopesheet. *Partial (2026-10-04)*: transform keys (`TransformKey`, hold/linear/ease, geometric scale) in source time, evaluated by `timeline::evaluate`/`transform_at`; Crop drawer "Add key" and Ken Burns. Open: other parameters (opacity, color), key markers on clips, dopesheet.
- [x] Transitions (dissolve, dip, wipe) — ahead of order (2026-10-03, ADR-0011): clip-owned, centered on the cut, limited by media handles, audio crossfade; storyline UI.
- [ ] Intent presets (PiP, split screen, cutaway) producing editable layers.
- [ ] After M7 save/export, pilot editable rectangle/ellipse masks and a focus/zoom preset on screen recordings; require undo/redo, save/load, frame-by-frame decoded-export inspection and measured preview deadlines. Use opaque cover for sensitive regions; do not present blur/pixelation as reliable redaction. [Research and comparators](Research/post-production-effects.md).
- [ ] Markers in the UI.
- [ ] Proxies selected by measured storage/seek/quality/compatibility, including compute-decoded candidates, and proxy/original switching. Treat generated proxies as disposable representations of a stable original; verify missing/deleted-proxy fallback and make export resolve original media or fail clearly, independent of viewer mode.
- [ ] Background render filling the cache where real time is not possible. Evaluate it separately from source proxies on the same effects-heavy scene: a proxy reduces source decode work, while a preview render can reuse already composited/effected regions. Bound disk use and invalidate only affected ranges; a deleted cache must not change project or export content.
- [ ] Evaluate semantic storyboard ↔ timeline zoom against ordinary zoom/minimap, including linked tracks and unequal durations; no historical behavior claim is assumed.

**Research**: [capcut](Research/capcut.md), [imovie](Research/imovie.md), [final-cut-pro](Research/final-cut-pro.md), [movie-maker](Research/movie-maker.md), [other-editors](Research/other-editors.md).

### M9 — Interop and color

- [ ] `libs/project-ir` + importer framework + import report + `Docs/interop.md` with levels per format.
- [ ] OTIO pilot first using a pinned library/schema and actual versioned exports; evaluate FCPXML, EDL and Kdenlive subsets with fixtures. For FCPXML, pin an exported version, cover both `.fcpxml` and applicable `.fcpxmld` bundles, and check rational timing, connected clips, gaps, media references and unsupported objects before claiming any support level. Report floating-to-rational conversion loss and unsupported effects.
- [ ] `oma-project diff | convert`.
- [ ] Color management presets, `.cube` LUTs, scopes; HDR gated by independent vectors and an actual display/preview/export contract.

**Research**: [davinci-resolve](Research/davinci-resolve.md), [premiere-pro](Research/premiere-pro.md), [final-cut-pro](Research/final-cut-pro.md), `CLAUDE.md` §16.

---

### Conditional pilot — Third-party effects marketplace (after M7/M8)

The maintainer requested a place for others to publish production/post-production
effects (2026-10-04). This is an explicit future scope direction under
`CLAUDE.md` §2; the public extension architecture still needs an ADR. Start
with a curated catalog of **data-only presets** built from native operations,
so installed packs create editable project content without loading external code.

- [ ] Define a versioned preset manifest with stable ID/author/license, contents, preview, compatible OmaMovie range and asset hashes. Pilot local install/update/disable, project save/load and missing-pack recovery. Show source and version before installation; keep old project rendering when a look leaves the catalog.
- [ ] Compare installing a pack with manually reproducing the same effect on target tasks. Measure discovery, completion, corrections and export fidelity; expand the catalog only if it improves that workflow.
- [ ] Write an ADR and prototype one out-of-process executable effect before accepting code submissions. Specify parameters, color/alpha/time, threading, deterministic export, version pinning, crash/timeout behavior, permissions, update rollback and missing-effect bypass. Measure CPU↔GPU transfer and p95/p99 preview time; prove any Vulkan handoff separately. Evaluate OpenFX against this contract rather than promising universal plugin compatibility.
- [ ] Publish submission/review rules and a versioned compatibility test suite before listing third-party binaries. Catalog review is not a sandbox or a guarantee of safe execution.

**Research**: [effect research and marketplace alternatives](Research/post-production-effects.md).

---

### Conditional pilot — Agent-assisted editing through MCP (after M7)

This is a research gate, not a committed v0.1 deliverable. Use the concrete
exact-time edit task and alternatives in [MCP research](Research/agentic-mcp.md)
to decide whether MCP earns a place in the product. Write an ADR before adding
AI, a dependency or a new process boundary (`CLAUDE.md` §§2, 20–21).

- [ ] After durable project save/load, expose a local, client-launched `stdio` prototype scoped to one chosen project. Pin an MCP protocol/SDK revision and test a real external host; do not add an HTTP listener or embedded model for this pilot.
- [ ] Expose bounded read-only project/media/timeline data with stable IDs, exact rational times, pagination and a persisted content revision. Optional thumbnails require explicit count/resolution and project access limits; media metadata is untrusted content.
- [ ] Add `propose_edit` for a small typed command set, dry-run validation on a copy and a structured before/after diff. Reject unknown commands, invalid ranges and stale revisions. Show the diff in OmaMovie; apply an accepted batch via the existing `Editor` as one undo transaction, with a fresh revision check. The ADR must settle GUI versus headless ownership and retry behavior before any mutating MCP tool is exposed.
- [ ] Compare the same synthetic exact-time tasks with GUI and deterministic `oma-project` CLI/macro baselines. Require correct final timeline, no unintended edit, one-step undo, stale-edit rejection and no action from adversarial media metadata. Record host/model/version, tool calls, corrections, elapsed time, privacy exposure and failures; only expand to representative user tasks if the pilot passes.
- [ ] Revisit background export/proxy jobs only after M7/M8 job APIs exist. Use the MCP Tasks extension if a chosen host supports it and the job needs durable polling/cancellation; short edit proposals do not need Tasks.

If CLI or GUI completes the task just as well with lower cost, stop at the
deterministic command surface. No agentic capability is validated by this plan.

---

## 4. Decisions and ADRs

| Decision | When | ADR | Initial proposal |
|---|---|---|---|
| Build, C++23, test framework, license | M0 | 0001 | **Accepted**: GNU Make, C++23, Cest |
| Time representation + speed curves | M0 | 0002 | **Accepted**: `RationalTime` + piecewise monotonic `TimeMap` |
| Threading and job system | M0 | 0003 | **Accepted**: pipeline threads + `JobPool` |
| GPU frame, synchronization, decode policy | M2 | 0004 | **Accepted**: wrapped `AVVkFrame`, VA-API → Vulkan Video → software |
| Qt Quick ↔ compositor | M1 | 0005 | Single device through `fromDeviceObjects` |
| Color space + libplacebo | M1 | 0006 | Linear float16; libplacebo depending on S6 |
| Project format | M7 | 0007 | **Accepted**: versioned pretty JSON, simdjson, `Timeline::restore` |
| ProjectIR and preservation | M9 | 0008 | `CLAUDE.md` §16 |
| Cache | M7 | 0009 | Key derived from inputs, `$XDG_CACHE_HOME/omamovie` |
| Canvas coordinates | M8 | 0010 | Decide before the aspect ratio becomes changeable |
| Transitions | ahead of M8 | 0011 | **Accepted**: clip-owned, centered on the cut, media handles |

ADR-0004 already records application-owned device/frame/decode choices; do not restart their
approval as if nothing had been implemented. S4/ADR-0005 cover the shared-device
import diagnostic; the interactive GUI still uses software-frame uploads.
M4 on-screen GPU handoff and S6/ADR-0006 remain pending.
The review corrects the rationale without silently changing accepted decisions. Any resulting
architecture change gets a new ADR under the existing convention.

---

## 5. Tests and quality per milestone

| Milestone | Mandatory |
|---|---|
| M0 | Time tests; sanitizers in CI |
| M2 | Media fixtures; probe fuzzing; frame lifetime test |
| M3 | Render graph without a GPU; GPU × CPU comparison |
| M4 | TSan; measured A/V drift |
| M5 | Every operation and undo transaction |
| M6 | Measured UI responsiveness and task-baseline comparison; partial theme update test |
| M7 | Save/load, migrations, failure injection + directory durability; loader fuzzing; independently checked export |
| M9 | Fuzzing of every importer; small fixtures per compatibility level |

**Long-form gate (proposed, after M7 export, before claiming support):** run the
[declared workload set and protocol](Research/long-form-editing.md#proposed-gate-before-optimizations)
on the reference Intel machine. Include a two-hour source with few cuts, a synthetic
5,000-clip timeline, repeated seeks in long-GOP 4K media, and a costly-effects control.
Record p50/p95/p99 and worst interaction and seek latencies, frame drops, project
open/save phases, peak RAM, cache disk use, decoded-export content/A/V checks and
recovery after failed save/export. The proposed 5,000-clip interaction criterion is
p95 < 100 ms and no routine edit stall > 250 ms; change it only as an explicit
product decision. A passing synthetic gate is evidence for the declared workloads,
not for every long video or hardware configuration. Use the observed dominant cost
to choose between QML model work, decoder/proxy work, preview rendering or project
storage changes. This gate has not run.

Benchmarks (`tools/bench`) always record hardware, driver, FFmpeg version, codec and decode path (`CLAUDE.md` §23).

---

## 6. Hardware matrix

| Hardware | Available | Used in |
|---|---|---|
| Intel Iris Xe (TigerLake) | **Yes** (development machine) | M1–M7, "modest hardware" target |
| AMD (RADV) | No | S8; needed before v0.1 |
| NVIDIA (nvidia-open) | No | S7, NVDEC/NVENC, interop; needed before v0.1 |
| Hybrid laptop | No | Device selection |
| CI (container, no GPU) | Yes | Software paths |

**Known blocker**: v0.1 promises Intel, AMD and NVIDIA. Without AMD and NVIDIA machines (or
contributors with that hardware), v0.1 can only declare Intel as validated.

---

## 7. Risks

| Risk | Milestone | Mitigation |
|---|---|---|
| Queue synchronization between Qt, FFmpeg and the compositor | M4 | Extend S4's stable-frame lock to FFmpeg playback and pause/drain workers for every Qt swapchain lifecycle event; ADR-0005 |
| Driver differences in DMA-BUF/modifiers | M1–M2 | Verify mapping/device identity; log explicit copy or software+upload fallback |
| No AMD/NVIDIA hardware | M7 | Declare what was validated; look for testers early |
| CUDA handle-specific synchronization | Future | Pin API/handle semantics and test scheduler liveness |
| Slow Vulkan-Hpp compilation | M2 | Confined include + PCH |
| Scope growth across milestones | All | Keep each milestone's acceptance criteria explicit; record scope changes before implementing them |
| Qt 6 without CMake (moc, rcc, QML type registration) | M4–M6 | Make rules with `pkg-config Qt6*`; validate with a minimal window in M4 before the UI (ADR-0001) |

---

## 8. Next step

M4 now has audio-clock playback and the viewer's GPU handoff (see M4). What remains to close it:
1. **Measure** before claiming real time: A/V drift over 10 minutes (the predeclared pilot bound),
   and new-frame time on the render thread at 1080p30/60 with the decode path recorded (S3/§23).
2. **Hardware decode in the viewer**: run the M4 pilot against software decode/upload. VA-API/Vulkan
   Video frames from a decode thread need ADR-0005 admission (pause/drain around Qt's swapchain
   lifecycle) because they touch the device outside the render thread; bounded decode-ahead queues
   come with it. Select the default from end-to-end results, not isolated decoder throughput.
3. J/K/L speeds and reverse: reverse no longer seeks per frame (M4); restarting playback still
   reopens every decoder.
4. Per-frame allocations on the viewer path (CLAUDE.md §23): `ViewerFrame`, the render graph's
   layers and curve points, and `timeline::evaluate` copying each layer's `VideoProperties`
   (curves, transform keys) are allocated every frame. Not measured as a cost yet; reuse the
   frame and pass evaluated values instead of copies when a profile shows it.

In parallel, M7's project format (ADR-0007) can start: the timeline model and the media library
are stable enough to persist. Resolve **S6/ADR-0006** before any color claim beyond the sRGB
preview transform. Schedule S7/S8 on real hardware before declaring AMD/NVIDIA paths validated.
