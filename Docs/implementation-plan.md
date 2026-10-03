# Implementation plan — OmaMovie

> Created on 2026-10-02; evidence reviewed on 2026-10-03 with skeptical-research. Engineering rules and invariants live in
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
| Contextual/magnetic/preset UI | Documented feature examples; effectiveness untested | Preserve design choices; M6 baseline/task pilot before claiming usability |
| Shared Vulkan device | ADR-0004 headless queues; S4/ADR-0005 validated Qt zero-flag queues for a static preview | M4 must pause live producers across Qt swapchain changes |
| Color | Current SDR subset; independent oracle and display stages pending | S6/ADR-0006; no HDR claim from RGBA16F storage |
| NVIDIA | Packaged FFmpeg runtime candidate; no hardware validation | S7; no universal speed ranking or toolkit-free build claim |
| OpenCL excluded | Existing maintainer scope decision; generic interop exists | Keep exclusion; no unsupported impossibility rationale |

Milestone checkboxes record implemented deliverables. Validation requires separate evidence.
S1/S2 and M0/M3 numbers below are prior reports, not rerun by the documentation audit.
New acceptance criteria are proposed pilot criteria set before future measurements; changes
to them must be recorded, rather than choosing thresholds after seeing results.

---

## 2. Releases

Every release is usable by a real audience; none is a tech demo.

| Release | Name | For whom | Summary |
|---|---|---|---|
| **v0.1** | First cut | People who record and edit simple videos (vlogs, tutorials, screen recordings) | Import (including Omarchy recordings), multitrack timeline, split/trim/ripple, transform/crop/opacity, audio gain/fades, simple titles, undo/redo, J/K/L, project save, H.264/HEVC/AV1 export with capability-dependent hardware paths and software fallback, Omarchy theme |
| **v0.2** | Creator | Short/vertical video, YouTube, social media | Captions (own track, SRT/VTT), canvas aspect ratio (vertical/horizontal), speed and speed ramps with presets, keyframes, transitions, presets (PiP, split screen, cutaway), markers, proxies, background render, storyboard↔timeline zoom evaluation |
| **v0.3** | Interop and color | People coming from Resolve/Kdenlive/FCP | OTIO, FCPXML, EDL, Kdenlive import; import report with levels; color management presets, `.cube` LUTs, scopes; HDR only after display/export validation |
| **Future** | — | — | `.prproj` import, CUDA backend for effects, beat sync, silence removal, Omarchy shell widget, upstream proposals to Omarchy |

The non-goals (`CLAUDE.md` §2) apply to every release.

---

## 3. Milestone overview

```
M0 Foundation ─┬─────────────────────────────────────────────┐
               │                                             │
M1 Hardware spikes (S1–S6, in parallel with the end of M0)    │
               │                                             │
M2 libs/gpu + libs/media ── M3 Compositor ── M4 Playback/audio│
                                    │                        │
M5 Timeline model (can start after M0, no GPU) ──────────────┘
                                    │
                     M6 UI v0.1 ── M7 Project + export ──► v0.1
                                                          │
                                    M8 Creator ─────────► v0.2
                                    M9 Interop and color ► v0.3
```

M5 depends only on `libs/base` and can progress in parallel with M2–M4.

---

## 4. Milestones

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
| **S3** Decode/encode paths | VA-API vs. QSV (vs. flagged Vulkan Video, for reference) per codec on the Iris Xe; FFv1 vs. ProRes for proxies | Frame time, CPU and power per codec; written selection policy | ADR-0004 |
| **S4** Qt Quick on the same device ✅ | Can Qt Quick present a compositor image on the same device without a render-path readback? | The repeatable offscreen diagnostic checks identical physical/logical device and queue, borrowed compositor-image pixels and 60 frames with no validation errors. A prior static visible experiment passed bounded queue contention. Its test-pattern window was replaced after user feedback: `make run-gui` now opens real videos with a temporary software-frame upload. Production on-screen GPU handoff belongs to M4. See [S4](spikes/S4-qt-shared-device.md) | ADR-0005 |
| **S5** Minimal compositing | 2 videos + 1 image with transform/crop/opacity and YUV→RGB | 1080p60 frame time measured and recorded | M3 |
| **S6** libplacebo | Does it operate on OmaMovie's images/device without copies? Quality and cost? | Decision "use libplacebo for color/scaling/LUT or our own shaders" | ADR-0006 |

S7 (NVIDIA path) and S8 (AMD) need other hardware; they are scheduled for when a machine is available (§7).

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
- [ ] `ComputeBackend` interface for effects (`VulkanComputeBackend` + `CpuBackend`), with the first effect; non-real-time sections are reported.
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
- [x] Timeline audio mix: gain, linear fades and mute per clip, sample-accurate clip boundaries, any number of overlapping clips (`oma::audio::mix_planar`, tested); clip audio at speeds other than 1 stays silent until time-stretching exists; mixing lives in the app (`TimelineAudio`) until a `playback` library is decided.
- [ ] Playback scheduler: decode ahead with bounded queues, cancellation on seek. *Partial*: audio renders 0.25 s ahead into the bounded ring and restarts on seek/edit; video still decodes one frame at a time on request.
- [ ] J/K/L (multiple speeds), frame step. *Partial*: frame step (←/→, Shift for 10) lands on exact rational frame times; K pauses and L plays at normal speed; reverse and faster speeds remain.
- [x] On-screen compositor-image handoff for the viewer (2026-10-03): the timeline's `Composition` becomes a `RenderGraph` with every visible layer and its fit/crop/transform/opacity/blend; `PreviewItem` composites and encodes on Qt's render thread and Qt samples the compositor's own `VkImage` (no readback, no CPU conversion). Decoding is still software (one plane upload per frame, CLAUDE.md §7.3); hardware frames need queue admission for producers on other threads. The smoke check verifies that the viewer shows composited video; a run under the Vulkan validation layer with synchronization validation reports no errors when no window capture is taken (Qt's `grabWindow` itself triggers swapchain hazards). *Not measured yet*: 1080p60 new-frame time on the render thread.
- [x] Qt Quick editor shell through `make run-gui`: Projects/Edit, media library, viewer, responsive timeline, play/pause and seek. The storyline is the M5 `Editor` (commands and undo/redo through `Session`); the viewer evaluates the timeline at the playhead, decodes forward without reseeking during playback and follows the audio clock. Decode-ahead queues remain pending.

**Done when**: A/V drift measured over 10 minutes against the proposed predeclared pilot bound in the audit; frame presentation and device latency are accounted for, with no growing drift. Seeks, speed changes, underruns and device changes recover; UI stays responsive; TSan clean. TSan does not prove RT deadline safety.

**Research**: `CLAUDE.md` §12–13.

---

### M5 — Timeline model (in parallel with M2–M4)

- [x] Tracks (video, audio; caption type already in the model), clips by stable media ID.
- [x] Commands with a single history transaction: insert, overwrite, append, split, trim (start/end, with or without ripple), roll, slip, slide, move, delete, ripple delete (clip and range), speed, video/audio properties, tracks, markers; user transactions are all-or-nothing.
- [x] Undo/redo; invariant `validate()`, checked by `Editor::execute` after every command (a rejected edit leaves the timeline and history untouched).
- [ ] Magnetism as a **UI policy** over generic commands.
- [x] Markers in the model.
- [ ] Clip time mapping per ADR-0002 plus a complementary interpolation/domain ADR: constant speed first; freeze/reverse have explicit timeline duration, ramps integrate speed with explicit rounding. **Constant speed is implemented** (`TimeMap::constant`); freeze, reverse and ramps wait for the complementary ADR.
- [x] Per-instant evaluation for a time `t` (`oma::timeline::evaluate` → `Composition`), see the deviation below.
- [x] Commands with no UI dependency; command serialization only when a concrete persistence/scripting requirement is established (undo alone does not require it).

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

**Research**: [premiere-pro](Research/premiere-pro.md), [imovie](Research/imovie.md), [davinci-resolve](Research/davinci-resolve.md).

---

### M6 — UI v0.1

Reference design: `Docs/ui-design.md`.

- [ ] Qt Quick shell: MediaPanel, PreviewPanel, TimelineView, Inspector, TransportControls.
- [ ] MediaPanel with a **"Recordings"** source (respects `OMARCHY_SCREENRECORD_DIR`, then `$XDG_VIDEOS_DIR`/fallback; handles growing and no-audio files); thumbnails and waveforms through the job system + invalidatable cache.
- [ ] TimelineView: tracks, drag, trim, split, snapping, **mouse-centered** zoom, overview bar (minimap). *Partial*: storyline with ripple trim by edge drag, split, scrubbing playhead, minimap and zoom; more tracks, clip dragging, snapping and mouse-centered zoom remain.
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

**Research**: [imovie](Research/imovie.md), [movie-maker](Research/movie-maker.md), [other-editors](Research/other-editors.md), [omarchy-integration](Research/omarchy-integration.md).

---

### M7 — Project, export and the v0.1 release

- [ ] ADR-0007 (project format) and ADR-0009 (cache).
- [ ] Versioned serialization (DTOs separate from the model), chained migrations, atomic replacement (same-filesystem temp, file sync, rename, parent-directory sync with errors surfaced), separate autosave, relink by fingerprint.
- [ ] SQLite media index, if justified in the ADR.
- [ ] Export: render graph → encoder (Vulkan/VA-API on Intel/AMD, NVENC on NVIDIA, software fallback) + mux, as a background job with progress and a notification (D-Bus).
- [ ] `oma-project inspect | validate | dump` for the native format.
- [ ] Packaging: `omastore.toml`, GitHub release with binaries, the app installed by `package()` (the `-git` PKGBUILD and dependency declaration already exist).

**Done when**: save/load round trip without differences; loader fuzzing and save-failure injection pass;
exported file checked with `ffprobe`, independent decode/content inspection and measured A/V sync; installation from the PKGBUILD on a
clean Omarchy machine.

**→ v0.1 release**

---

### M8 — v0.2 Creator

- [ ] Caption track, SRT/VTT import/export.
- [ ] Changeable canvas aspect ratio (ADR: normalized coordinates vs. pixels).
- [ ] Constant speed + speed ramps with named presets (curve from ADR-0002).
- [ ] Keyframes with interpolation on every animatable parameter; simple dopesheet.
- [ ] Transitions (dissolve, dip, wipe).
- [ ] Intent presets (PiP, split screen, cutaway) producing editable layers.
- [ ] Markers in the UI.
- [ ] Proxies selected by measured storage/seek/quality/compatibility, including compute-decoded candidates, and proxy/original switching.
- [ ] Background render filling the cache where real time is not possible.
- [ ] Evaluate semantic storyboard ↔ timeline zoom against ordinary zoom/minimap, including linked tracks and unequal durations; no historical behavior claim is assumed.

**Research**: [capcut](Research/capcut.md), [imovie](Research/imovie.md), [movie-maker](Research/movie-maker.md), [other-editors](Research/other-editors.md).

### M9 — v0.3 Interop and color

- [ ] `libs/project-ir` + importer framework + import report + `Docs/interop.md` with levels per format.
- [ ] OTIO pilot first using a pinned library/schema and actual versioned exports; evaluate FCPXML, EDL and Kdenlive subsets with fixtures. Report floating-to-rational conversion loss and unsupported effects.
- [ ] `oma-project diff | convert`.
- [ ] Color management presets, `.cube` LUTs, scopes; HDR gated by independent vectors and an actual display/preview/export contract.

**Research**: [davinci-resolve](Research/davinci-resolve.md), [premiere-pro](Research/premiere-pro.md), `CLAUDE.md` §16.

---

## 5. Decisions and ADRs

| Decision | When | ADR | Initial proposal |
|---|---|---|---|
| Build, C++23, test framework, license | M0 | 0001 | **Accepted**: GNU Make, C++23, Cest |
| Time representation + speed curves | M0 | 0002 | **Accepted**: `RationalTime` + piecewise monotonic `TimeMap` |
| Threading and job system | M0 | 0003 | **Accepted**: pipeline threads + `JobPool` |
| GPU frame, synchronization, decode policy | M2 | 0004 | **Accepted**: wrapped `AVVkFrame`, VA-API → Vulkan Video → software |
| Qt Quick ↔ compositor | M1 | 0005 | Single device through `fromDeviceObjects` |
| Color space + libplacebo | M1 | 0006 | Linear float16; libplacebo depending on S6 |
| Project format | M7 | 0007 | The file is the source of truth, diffable |
| ProjectIR and preservation | M9 | 0008 | `CLAUDE.md` §16 |
| Cache | M7 | 0009 | Key derived from inputs, `$XDG_CACHE_HOME/omamovie` |
| Canvas coordinates | M8 | 0010 | Decide before the aspect ratio becomes changeable |

ADR-0004 already records application-owned device/frame/decode choices; do not restart their
approval as if nothing had been implemented. S4/ADR-0005 cover the shared-device
import diagnostic; the interactive GUI still uses software-frame uploads.
M4 on-screen GPU handoff and S6/ADR-0006 remain pending.
The review corrects the rationale without silently changing accepted decisions. Any resulting
architecture change gets a new ADR under the existing convention.

---

## 6. Tests and quality per milestone

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

Benchmarks (`tools/bench`) always record hardware, driver, FFmpeg version, codec and decode path (`CLAUDE.md` §23).

---

## 7. Hardware matrix

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

## 8. Risks

| Risk | Milestone | Mitigation |
|---|---|---|
| Queue synchronization between Qt, FFmpeg and the compositor | M4 | Extend S4's stable-frame lock to FFmpeg playback and pause/drain workers for every Qt swapchain lifecycle event; ADR-0005 |
| Driver differences in DMA-BUF/modifiers | M1–M2 | Verify mapping/device identity; log explicit copy or software+upload fallback |
| No AMD/NVIDIA hardware | M7 | Declare what was validated; look for testers early |
| CUDA handle-specific synchronization | Future | Pin API/handle semantics and test scheduler liveness |
| Slow Vulkan-Hpp compilation | M2 | Confined include + PCH |
| Scope growing before v0.1 | All | Nothing from v0.2 lands before v0.1 ships |
| Qt 6 without CMake (moc, rcc, QML type registration) | M4–M6 | Make rules with `pkg-config Qt6*`; validate with a minimal window in M4 before the UI (ADR-0001) |

---

## 9. Next step

M4 now has audio-clock playback and the viewer's GPU handoff (see M4). What remains to close it:
1. **Measure** before claiming real time: A/V drift over 10 minutes (the predeclared pilot bound),
   and new-frame time on the render thread at 1080p30/60 with the decode path recorded (S3/§23).
2. **Hardware decode in the viewer**: VA-API/Vulkan Video frames from a decode thread need the
   ADR-0005 admission (pause/drain around Qt's swapchain lifecycle) because they touch the device
   outside the render thread; bounded decode-ahead queues come with it.
3. J/K/L speeds and reverse.

In parallel, M7's project format (ADR-0007) can start: the timeline model and the media library
are stable enough to persist. Resolve **S6/ADR-0006** before any color claim beyond the sRGB
preview transform. Schedule S7/S8 on real hardware before declaring AMD/NVIDIA paths validated.
