# Implementation plan — OmaMovie

> Created on 2026-10-02 from `Docs/Research/`. Engineering rules and invariants live in
> `CLAUDE.md`; this document defines **what to build, in which order and how to know it is
> done**.
>
> Update this file when a milestone is finished (tick the items, record deviations).
> Structural divergences need an ADR (`CLAUDE.md` §20).

---

## 1. Goal

Build Omarchy's native video editor for **every kind of content creator**: it opens phone
video and Omarchy screen recordings directly, plays in real time on an integrated GPU, works
offline, without an account and without a paywall, with the simplicity of iMovie/CapCut on
top of an engine that can grow to Resolve's level.

Research foundations:

| Finding | Document | Effect on the plan |
|---|---|---|
| The free Resolve on Linux cannot open H.264/AAC and only officially supports NVIDIA | `davinci-resolve.md` §11 | Decoding consumer formats on Intel/AMD/NVIDIA is a v0.1 priority |
| Kdenlive is Omarchy's default editor | `omarchy-integration.md` §1 | Goal: become Omarchy's default editor |
| Omarchy recordings (ALT+PRINT): H.264/HEVC/AV1, 60 fps CFR, AAC, in `~/Videos` | `omarchy-integration.md` §5 | "Recordings" source already in v0.1 |
| Stability and lost work are the main complaint against Premiere, CapCut and Shotcut | `premiere-pro.md` §10, `capcut.md` §3 | Atomic save, autosave and fuzzing from the first format |
| Contextual inspector, intent presets, magnetic timeline as a policy | `imovie.md`, `capcut.md` | UI design for v0.1/v0.2 |
| One Vulkan device shared by FFmpeg, the compositor and Qt | `vulkan.md`, `hardware-strategy.md` | Spikes before the definitive code |
| NVDEC/NVENC without the CUDA toolkit; CUDA kernels only with a measured gain | `cuda.md` | CUDA at runtime, optional build, late phase |
| OpenCL removed | `opencl.md` | Compute = Vulkan + CPU (+ CUDA later) |

---

## 2. Releases

Every release is usable by a real audience; none is a tech demo.

| Release | Name | For whom | Summary |
|---|---|---|---|
| **v0.1** | First cut | People who record and edit simple videos (vlogs, tutorials, screen recordings) | Import (including Omarchy recordings), multitrack timeline, split/trim/ripple, transform/crop/opacity, audio gain/fades, simple titles, undo/redo, J/K/L, project save, hardware H.264/HEVC/AV1 export, Omarchy theme |
| **v0.2** | Creator | Short/vertical video, YouTube, social media | Captions (own track, SRT/VTT), canvas aspect ratio (vertical/horizontal), speed and speed ramps with presets, keyframes, transitions, presets (PiP, split screen, cutaway), markers, proxies, background render, storyboard↔timeline zoom |
| **v0.3** | Interop and color | People coming from Resolve/Kdenlive/FCP | OTIO, FCPXML, EDL, Kdenlive import; import report with levels; color management presets, `.cube` LUTs, scopes; HDR |
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
- [x] Build packages: `make gcc clang ffmpeg` (M0). M1 still needs `vulkan-headers vulkan-tools libva-utils`.
- [x] Non-recursive `Makefile` (GNU Make, ADR-0001): `BUILD=debug|release|asan|tsan`, one `module.mk` per lib and per test suite.
- [x] Full warning set with `-Werror` on the libs; `.clang-format`, `.clang-tidy` (warnings are errors), `.editorconfig`, `.gitignore`.
- [x] **Dependency graph from §5.2 enforced by Make**: a forbidden dependency or an unregistered lib fails before compiling. Only `libs/base` exists; the other libs arrive with their milestones.
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

**Research**: `CLAUDE.md` §6, `capcut.md` §4.2 (speed curves).

---

### M1 — Hardware spikes

Code in `tools/spikes/`, disposable; whatever proves its value moves to the libs in M2/M3.
Results in `Docs/spikes/Sn-<name>.md` with numbers, driver versions and a conclusion.

| Spike | Question | Done when | Feeds |
|---|---|---|---|
| **S1** Inventory ✅ | Which codecs/profiles does the Iris Xe decode and encode through Vulkan Video, VA-API and QSV? | Codec × API × bit depth table — **done**, see `Docs/spikes/S1-hardware-inventory.md`: VA-API is the default decode path; Vulkan Video is behind `ANV_DEBUG` on TigerLake; Vulkan compute codecs (ProRes, FFv1) work without flags | ADR-0004 |
| **S2** FFmpeg on OmaMovie's device ✅ | Does FFmpeg 9 accept a `VkDevice` created with Vulkan-Hpp and deliver `AVVkFrame` without readback? How does queue locking work? | H.264/HEVC/AV1 decoded into `AVVkFrame`, zero copy to the CPU confirmed — **done**, see `Docs/spikes/S2-ffmpeg-own-device.md`: VA-API → DMA-BUF → Vulkan on our device, exact luma, no `AV_HWFRAME_MAP_DIRECT`; queues via `VK_KHR_internally_synchronized_queues` | ADR-0004 |
| **S3** Decode/encode paths | VA-API vs. QSV (vs. flagged Vulkan Video, for reference) per codec on the Iris Xe; FFv1 vs. ProRes for proxies | Frame time, CPU and power per codec; written selection policy | ADR-0004 |
| **S4** Qt Quick on the same device | Do `QQuickGraphicsDevice::fromDeviceObjects` + `QSGVulkanTexture::fromNative` show the compositor image without a copy? | 60 fps preview, no deadlock between Qt, FFmpeg and the compositor | ADR-0005 |
| **S5** Minimal compositing | 2 videos + 1 image with transform/crop/opacity and YUV→RGB | 1080p60 frame time measured and recorded | M3 |
| **S6** libplacebo | Does it operate on OmaMovie's images/device without copies? Quality and cost? | Decision "use libplacebo for color/scaling/LUT or our own shaders" | ADR-0006 |

S7 (NVIDIA path) and S8 (AMD) need other hardware; they are scheduled for when a machine is available (§7).

**Done when**: ADR-0004, 0005 and 0006 are written with data from the spikes.

**Research**: `hardware-strategy.md` §5, `vulkan.md` §5–7.

---

### M2 — `libs/gpu` and `libs/media`

**`libs/gpu`**
- [ ] `VkInstance`/`VkDevice` creation with Vulkan-Hpp (`vk::raii`, `VULKAN_HPP_NO_EXCEPTIONS` + `VULKAN_HPP_RAII_NO_EXCEPTIONS` → `std::expected`), include confined to `.cpp` files + PCH.
- [ ] Extension list: interop (DMA-BUF, DRM modifiers, external memory/semaphore fd), video (decode/encode), those Qt requires.
- [ ] A single queue lock shared with FFmpeg and Qt.
- [ ] VMA for our allocations; a separate path for imported memory.
- [ ] Timeline semaphore wrappers; ordered destruction (consumers → pools → device).
- [ ] Capability table + device selection (hybrid laptops: one primary device; do not wake the dGPU needlessly).

**`libs/media`**
- [ ] Probe → our own structure (streams, codec, timebase, duration, color, rotation, audio layout).
- [ ] Demux, software decode (correctness reference).
- [ ] Hardware decode with the per-codec/driver policy from S3 (Vulkan Video, mapped VA-API, NVDEC on NVIDIA), logged software fallback.
- [ ] `Frame` wrapping `AVVkFrame` (timeline semaphore contract), frame pool, no per-frame allocation in steady state.
- [ ] Accurate seek by PTS (VFR), `AV_NOPTS_VALUE` converted to `std::optional`.
- [ ] Audio decode + resampling to planar float32.
- [ ] Compute-shader intermediates (FFv1/ProRes) verified in FFmpeg 9.

**Tests**: CFR/VFR, 8/10-bit, H.264/HEVC/AV1, rotation, AAC/Opus and truncated fixtures;
**a real Omarchy recording** (generated locally); a fuzzing target for the probe.

**Done when**: real-time 1080p60 H.264/HEVC/AV1 decode on the Iris Xe **without readback**,
measured with `oma-bench`; seek tests pass on VFR.

**Research**: `vulkan.md`, `cuda.md` §2–3, `omarchy-integration.md` §5.

---

### M3 — Compositor and render graph

- [ ] Render graph as plain data (testable without a GPU).
- [ ] Vulkan compositor: YUV→RGB reading matrix/range/primaries/transfer; transform, crop, opacity, basic blend modes; layer stack.
- [ ] Working space per ADR-0006 (linear float16 proposed); RGBA output for the preview.
- [ ] If S6 approves: libplacebo as a color/scaling stage.
- [ ] GLSL/Slang shaders → SPIR-V at build time; persisted pipeline cache.
- [ ] `ComputeBackend` with `VulkanComputeBackend` and `CpuBackend` (reference); effects declare GPU capability; non-real-time sections are reported.

**Done when**: the S5 scenario runs on library code; GPU × CPU comparison within tolerance;
1080p60 frame time with 3 layers measured and recorded.

**Research**: `CLAUDE.md` §9, `premiere-pro.md` §2 (render bar), `davinci-resolve.md` §5 (graph inside, stack outside).

---

### M4 — Playback and audio

- [ ] PipeWire output; real-time callback without allocation or contended locks.
- [ ] Master clock = audio; video drops/repeats frames.
- [ ] Playback scheduler: decode ahead with bounded queues, cancellation on seek.
- [ ] J/K/L (multiple speeds), frame step.
- [ ] Minimal Qt Quick window with the S4 preview (no final UI yet).

**Done when**: A/V drift **measured** over 10 minutes and a target set from the measurement;
seek and J/K/L never freeze the UI; TSan clean.

**Research**: `CLAUDE.md` §12–13.

---

### M5 — Timeline model (in parallel with M2–M4)

- [ ] Tracks (video, audio; caption type already in the model), clips by stable media ID.
- [ ] Commands with a single history transaction: insert, overwrite, split, trim, ripple, roll, slip, slide, delete, ripple delete.
- [ ] Undo/redo; invariant `validate()`.
- [ ] Magnetism as a **UI policy** over generic commands.
- [ ] Markers in the model.
- [ ] Clip time mapping per ADR-0002 (constant speed now, structure ready for curves).
- [ ] Render graph generation for a time `t`.
- [ ] Serializable commands with no UI dependency (future base for scripting/CLI).

**Done when**: every operation is tested, including combinations and undo/redo of
transactions; render graph snapshots for known states.

**Research**: `premiere-pro.md` §3, `imovie.md` §5, `davinci-resolve.md` §10.

---

### M6 — UI v0.1

Reference design: `Docs/ui-design.md`.

- [ ] Qt Quick shell: MediaPanel, PreviewPanel, TimelineView, Inspector, TransportControls.
- [ ] MediaPanel with a **"Recordings"** source (watches `$XDG_VIDEOS_DIR`); thumbnails and waveforms through the job system + invalidatable cache.
- [ ] TimelineView: tracks, drag, trim, split, snapping, **mouse-centered** zoom, overview bar (minimap).
- [ ] Contextual inspector: video (transform, crop, opacity), audio (gain, fades), text (simple title); indicator of the adjustments active on a clip.
- [ ] Direct manipulation in the preview (position, scale, crop) emitting commands.
- [ ] Central action system (id, name, shortcut), independent of the input device; full keyboard editing.
- [ ] Omarchy platform module (`apps/omamovie/src/platform/omarchy/`): theme through `omarchy-theme-color --all` + a watcher on `~/.local/state/omarchy/current/` + fallback; Omarchy font; app_id `omamovie`.
- [ ] `omamovie.desktop`, icon, documentation of the Hyprland opacity rule.
- [ ] Projects screen (recent projects + recent recordings with "Edit") and open-by-origin rules; single instance (`ui-design.md` §3.1).
- [ ] The Omarchy font across the UI, with live switching (`ui-design.md` §10.1).
- [ ] Initial set of our own SVG icons for v0.1 (`ui-design.md` §10.2).
- [ ] Shortcuts following the iMovie/Final Cut convention, checked against the official documentation (`ui-design.md` §8.1).

**Done when**: a short vlog can be edited from start to finish, including keyboard-only; the
UI thread never blocks (measured); switching the Omarchy theme applies live without touching
the preview.

**Research**: `imovie.md` §3–6, `movie-maker.md` §5 (Clipchamp layout), `other-editors.md` §3 (mouse-centered zoom), `omarchy-integration.md` §3–4.

---

### M7 — Project, export and the v0.1 release

- [ ] ADR-0007 (project format) and ADR-0009 (cache).
- [ ] Versioned serialization (DTOs separate from the model), chained migrations, atomic save, separate autosave, relink by fingerprint.
- [ ] SQLite media index, if justified in the ADR.
- [ ] Export: render graph → encoder (Vulkan/VA-API on Intel/AMD, NVENC on NVIDIA, software fallback) + mux, as a background job with progress and a notification (D-Bus).
- [ ] `oma-project inspect | validate | dump` for the native format.
- [ ] Packaging: PKGBUILD, `omastore.toml`, GitHub release with binaries.

**Done when**: save/load round trip without differences; loader fuzzing without crashes;
exported file validated with `ffprobe` and A/V in sync; installation from the PKGBUILD on a
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
- [ ] Proxies (compute-decoded intermediates) and proxy/original switching.
- [ ] Background render filling the cache where real time is not possible.
- [ ] Semantic storyboard ↔ timeline zoom.

**Research**: `capcut.md` §4.2, `imovie.md` §5, `movie-maker.md` §4, `other-editors.md` §1.

### M9 — v0.3 Interop and color

- [ ] `libs/project-ir` + importer framework + import report + `Docs/interop.md` with levels per format.
- [ ] OTIO import (first; Resolve exports it natively), FCPXML, EDL, Kdenlive (MLT XML).
- [ ] `oma-project diff | convert`.
- [ ] Color management presets, `.cube` LUTs, scopes; HDR.

**Research**: `davinci-resolve.md` §8, `premiere-pro.md` §5, `CLAUDE.md` §16.

---

## 5. Decisions and ADRs

| Decision | When | ADR | Initial proposal |
|---|---|---|---|
| Build, C++23, test framework, license | M0 | 0001 | **Accepted**: GNU Make, C++23, Cest |
| Time representation + speed curves | M0 | 0002 | **Accepted**: `RationalTime` + piecewise monotonic `TimeMap` |
| Threading and job system | M0 | 0003 | **Accepted**: pipeline threads + `JobPool` |
| GPU frame, synchronization, decode policy | M1 | 0004 | Wrapped `AVVkFrame`; selection per codec/driver |
| Qt Quick ↔ compositor | M1 | 0005 | Single device through `fromDeviceObjects` |
| Color space + libplacebo | M1 | 0006 | Linear float16; libplacebo depending on S6 |
| Project format | M7 | 0007 | The file is the source of truth, diffable |
| ProjectIR and preservation | M9 | 0008 | `CLAUDE.md` §16 |
| Cache | M7 | 0009 | Key derived from inputs, `$XDG_CACHE_HOME/omamovie` |
| Canvas coordinates | M8 | 0010 | Decide before the aspect ratio becomes changeable |

Pending approval before entering `CLAUDE.md` (`hardware-strategy.md` §4): Vulkan-Hpp without
exceptions, single device, CUDA policy, hybrid laptops, Omarchy integration. Proposal:
confirm them at the start of M1, together with ADR-0004.

---

## 6. Tests and quality per milestone

| Milestone | Mandatory |
|---|---|
| M0 | Time tests; sanitizers in CI |
| M2 | Media fixtures; probe fuzzing; frame lifetime test |
| M3 | Render graph without a GPU; GPU × CPU comparison |
| M4 | TSan; measured A/V drift |
| M5 | Every operation and undo transaction |
| M6 | UI thread never blocked; theme switching test |
| M7 | Save/load round trip; migrations; loader fuzzing; validated export |
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
| Queue synchronization between Qt, FFmpeg and the compositor | M1 (S4) | Spike before the UI; single lock; ADR-0005 |
| Driver differences in DMA-BUF/modifiers | M1–M2 | Use FFmpeg's mapping; logged GPU→GPU fallback |
| No AMD/NVIDIA hardware | M7 | Declare what was validated; look for testers early |
| CUDA does not accept wait-before-signal | Future | Order guaranteed by the scheduler |
| Slow Vulkan-Hpp compilation | M2 | Confined include + PCH |
| Scope growing before v0.1 | All | Nothing from v0.2 lands before v0.1 ships |
| Qt 6 without CMake (moc, rcc, QML type registration) | M4–M6 | Make rules with `pkg-config Qt6*`; validate with a minimal window in M4 before the UI (ADR-0001) |

---

## 9. Next step

Start **M1**: install `vulkan-headers vulkan-tools libva-utils` and run S1 (hardware
inventory with `vulkaninfo` and `vainfo`), then S2 (FFmpeg decoding on a `VkDevice` created
by OmaMovie).
