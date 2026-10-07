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

M0–M10 are construction and validation milestones toward v0.1, not separate releases.

```
M0 Foundation ─┬─────────────────────────────────────────────┐
               │                                             │
M1 Hardware spikes (S1–S6, in parallel with the end of M0)    │
               │                                             │
M2 libs/gpu + libs/media ── M3 Compositor ── M4 Playback/audio│
                                    │                        │
M5 Timeline model (can start after M0, no GPU) ──────────────┘
                                    │
                     M6 UI ── M7 Project + export ── M8 Creator ── M9 Interop + color ── M10 Validation ──► v0.1
```

M5 depends only on `libs/base` and can progress in parallel with M2–M4.
The available Iris Xe and S1–S6 results are sufficient to implement and validate the
Intel paths of M2 and M3 now. S7/S8 and hybrid-machine checks extend the hardware matrix;
they are not prerequisites for Intel M2/M3 work. M4 presentation measurements remain a
separate end-to-end gate.

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
| **S6** libplacebo ✅ | Does it operate on OmaMovie's images/device without copies? Quality and cost? | Decision "use libplacebo for color/scaling/LUT or our own shaders" — **done** (2026-10-04), see `Docs/spikes/S6-libplacebo.md`: works on our device and images without copies (zero-flag queues); our shaders kept (display encode within 1 level of the formula vs 4; equal or lower cost); libplacebo is the candidate for HDR tone mapping and export scaling. Found and fixed three `gpu`/`media` defects on the way (sync-queue extension on zero-flag devices broke VA-API mapping; swapchain without surface; per-plane VA-API image formats) | ADR-0006 |

S7 (NVIDIA path) and S8 (AMD) need other hardware; they are scheduled for when a machine is available (§6).

**Done when**: ADR-0004 evidence limits are recorded, and ADR-0005/0006 are written after S4/S6 pass.

**Status (2026-10-04)**: S1–S6 done on the Iris Xe; ADR-0004/0005/0006 written. S7 (NVIDIA) and S8 (AMD) wait for hardware, so M1 is complete for Intel only. Power per decode path was not measured (RAPL needs root).
See [validation protocols](Research/skeptical-review.md#validation-protocols); S1/S2 alone do not complete M1.

**Research**: [hardware-strategy](Research/hardware-strategy.md), [vulkan](Research/vulkan.md).

---

### M2 — `libs/gpu` and `libs/media`

**`libs/gpu`**
- [x] `VkInstance`/`VkDevice` creation with Vulkan-Hpp (`vk::raii`, `VULKAN_HPP_NO_EXCEPTIONS` + `VULKAN_HPP_RAII_NO_EXCEPTIONS` → `std::expected`), include confined to `.cpp` files + PCH.
- [x] Extension list: interop (DMA-BUF, DRM modifiers, external memory/semaphore fd), video (decode/encode), those Qt requires.
- [x] Queue synchronization for current FFmpeg/compositor consumers.
- [x] Qt queue retrieval and stable-frame submission on zero-flag queues verified in S4 with direct render-thread queue-lock signals.
- [ ] M4: pause/drain live GPU workers before Qt swapchain create/resize/teardown, including cancellation and device loss; frame signals alone do not enclose Qt's `vkDeviceWaitIdle`. *For the viewer*: solved by construction — the compositor submits from Qt's render thread during sync (the thread that also recreates the swapchain), with a recursive queue lock. *Producers on other threads (2026-10-05, ADR-0005)*: `gpu::Device` admission gate, closed by the Qt bridge between frames (where Qt changes its swapchain and idles the device) and opened for each frame; the hardware-decode producer is admitted per frame with a cancellable wait. Gate tested under TSan (`tests/gpu`); a validation-layer run of the hardware pilot reported no threading or synchronization errors. *Device loss (2026-10-07)*: loss is treated as final for the `VkDevice` (Qt renders with the same device, so replacing it needs a new window): `CommandRunner` marks the device on `VK_ERROR_DEVICE_LOST` (new `ErrorCode::DeviceLost`) and refuses later submissions, `admit()` refuses and releases waiters so the decode producer stops, and the viewer stops compositing and shows a lasting notice that the project is safe to save; editing and saving continue on the CPU. Tested with a fresh device in `tests/gpu` (also under TSan) and by a simulated loss at the end of the GUI smoke (notice shown, an edit saved). A real driver reset was not induced, and automatic recovery (a new device and window) is not attempted. The smoke also found that `Session::notice` was never displayed; the viewer now shows notices. Open: export once it runs beside the viewer.
- [x] Evaluate VMA against the existing `gpu` allocation wrappers; retain explicit imported-memory lifetime — *decided (2026-10-04), [evidence](Research/evidence/2026-10-04-decode-allocations.txt)*: the S5 scene makes 16 `vkAllocateMemory` calls (8 with VA-API frames), all at setup and all freed, none per frame; ANV allows 2³²−1 allocations. VMA's sub-allocation has nothing to fix, so it is not added. Revisit with the GPU frame cache (ADR-0009), which needs a VRAM budget (`VK_EXT_memory_budget` or VMA's budget queries).
- [x] GPU command/resource wrappers exist.
- [x] Ordered destruction and cancellation verified under concurrent consumers (consumers → pools → device) — *2026-10-04, `tests/compositor/test_lifetime.cpp`*: a job decodes (software and VA-API) into a bounded queue while the test thread composites; cancellation lands mid-stream, a full queue of frames is composited after its decoder was destroyed, then compositor → queue → job pool → device are destroyed in order. Clean under ASan and TSan. One producer and one consumer only; device loss is not covered. (The TSan build itself had been broken by a GCC 16 `-Wnull-dereference` false positive in libstdc++; the Makefile now drops that warning for `BUILD=tsan` only.)
- [ ] Capability table + device selection (hybrid laptops: identity matching and power behavior measured before claiming power-safe discovery). *Partial (2026-10-04)*: `DeviceInfo::render_node` comes from `VK_EXT_physical_device_drm`, is logged with the device, and VA-API now opens that node instead of libva's default, so decode and compositing use the same GPU (a warning names the fallback when a driver reports no node). `DeviceInfo` now reports per-device DMA-BUF, DRM modifier, external-semaphore and advertised Vulkan Video queue/codec prerequisites. The [Intel runtime check](Research/evidence/2026-10-04-m2-m3-intel-recheck.md#runtime-capability-report) confirms the interop extensions and no default Vulkan Video decode queue. This is a capability report, not a codec/profile guarantee; hybrid identity and power behavior still need a hybrid machine.

**`libs/media`**
- [x] Probe → our own structure (streams, codec, timebase, duration, color, rotation, audio layout).
- [x] Demux, software decode (correctness reference).
- [x] Hardware decode with the per-codec/driver policy (ADR-0004: mapped VA-API, Vulkan Video), logged software fallback.
- [ ] NVDEC on NVIDIA, once NVIDIA hardware is in the matrix.
- [x] `Frame` wrapping `AVVkFrame` (timeline semaphore contract).
- [x] Frame pool, no per-frame allocation in steady state — *measured and decided (2026-10-04), [evidence](Research/evidence/2026-10-04-decode-allocations.txt)*: pixel data already comes from FFmpeg's buffer pool and the VA-API surface pool; steady-state decoding makes 36 heap allocations per frame in software and 103 with VA-API, of which OmaMovie owns 2–3 (`VideoFrame::Impl`, the `AVFrame` shell, the mapped frame). The rest are FFmpeg reference wrappers and the iHD driver. An OmaMovie shell pool would remove about 5 % of them, so none is built; revisit if a profile shows allocator time on the decode thread.
- [x] Accurate seek by PTS (VFR), `AV_NOPTS_VALUE` converted to `std::optional`.
- [x] Audio decode + resampling to planar float32.
- [x] Compute-shader intermediates (FFv1/ProRes) verified for the actual build; compare storage/seek/quality before choosing proxy format — [S3](spikes/S3-decode-paths.md): ProRes LT (CPU encode, Vulkan or CPU decode) is the proxy candidate; FFv1 Vulkan decode is below real time; `prores_ks_vulkan` output is invalid at 1080p.

**Tests**: CFR/VFR, 8/10-bit, H.264/HEVC/AV1, rotation, AAC/Opus and truncated fixtures;
**a real Omarchy recording** (generated locally); a fuzzing target for the probe.

**Done when**: real-time 1080p60 H.264/HEVC/AV1 decode on the Iris Xe **without readback**,
measured end-to-end with the audit protocol (`oma-bench` is still a target tool); seek tests pass on VFR. Record actual frame format, copies, repetitions, drops and memory bounds.

**Status (2026-10-04, Intel only)**: decode through `libs/media` into OmaMovie's device, VA-API frames mapped without readback (NV12/P010 per-plane images): two 1080p60 streams per frame cost 3.1–3.2 ms p50 for H.264 + HEVC 10-bit and 2.1 ms p50 / 12.4 ms max for two AV1 streams ([S5 evidence](Research/evidence/2026-10-04-s5-minimal-compositing.txt)); decoder-only throughput in [S3](spikes/S3-decode-paths.md); VFR seek tests pass (`tests/media`). Memory: device memory fixed at setup (16 allocations, [evidence](Research/evidence/2026-10-04-decode-allocations.txt)), decode queues bounded. The [runtime capability check](Research/evidence/2026-10-04-m2-m3-intel-recheck.md) confirms that the selected Intel render node advertises the required interop but no Vulkan Video decode queue by default. Not measured: the end-to-end audit protocol with presentation (needs `oma-bench`/M4), repetitions and drops in a real player. Open M2 items need other hardware (NVDEC, hybrid laptops) or belong to M4 (swapchain admission).

**Research**: [vulkan](Research/vulkan.md), [cuda](Research/cuda.md), [omarchy-integration](Research/omarchy-integration.md).

---

### M3 — Compositor and render graph

- [x] Render graph as plain data (testable without a GPU).
- [x] Vulkan compositor: YUV→RGB with an SDR metadata subset; transform, crop, opacity, basic blend modes; layer stack.
- [x] Independent range/primaries/transfer/chroma validation; explicit rejection/degradation for unsupported PQ/HLG and a preview display transform. *Done for the SDR subset (2026-10-05)*: `VulkanCompositor::encode_display` applies the sRGB transfer function to the linear output (RGBA8 for an SDR display), tested against the formula within one 8-bit level. Quantized BT.601/BT.709/BT.2020 NCL primary-colour vectors independently check YUV matrix/range conversion at 8/10-bit (`tests/compositor/test_geometry_color.cpp`; [ITU-R BT.601-7](https://www.itu.int/rec/r-rec-bt.601-7-201103-i/en), [BT.709-6](https://www.itu.int/rec/R-REC-BT.709-6-201506-I), [BT.2020-2](https://www.itu.int/rec/r-rec-bt.2020-2-201510-i/en)). A decoded untagged SD YUV patch frame checks Vulkan output against FFmpeg's independent `libswscale` conversion after the declared BT.1886 decode (worst sampled linear difference 0.0222 on both llvmpipe and the [Intel Iris Xe](Research/evidence/2026-10-04-m2-m3-intel-recheck.md#independent-decoded-frame-color-check); `tests/compositor/test_compositors.cpp`). `ffprobe` reports its Y4M colour fields as unknown, so this exercises the BT.601 fallback. Tagged decoded formats (2026-10-05): the same patches as lossless FFV1 tagged BT.709 at limited and full range match `libswscale` within 0.0076 and 0.0056 on llvmpipe; misreading them as BT.601 would move the orange patch by 0.059, above the 0.025 tolerance. `VideoFrame::copy_rgba` (the oracle and the software thumbnail path) now passes the stream's matrix and range to `libswscale`, which had silently assumed BT.601 limited range. Chroma siting (2026-10-05): a reconstruction written in the test from the standards (left/center sample positions, bilinear, BT.601 equations, BT.1886) matches the CPU compositor exactly across a patch edge for both sitings on the decoded 8-bit 4:2:0 frame, and the GPU matches the CPU there; the two sitings differ by about 0.09 at the edge. PQ/HLG degrade to the SDR path with one decoder warning per file (ADR-0006, `resolve_transfer` test). Not covered: per-frame chroma-location changes, 10-bit or 4:2:2 siting, HDR tone mapping (M9).
- [x] Working space per the ADR-0006 proposal (linear BT.709, premultiplied, RGBA16F output for the preview).
- [x] ~~If S6 approves: libplacebo as a color/scaling stage.~~ S6/ADR-0006: not adopted now; candidate for HDR tone mapping and export scaling.
- [x] GLSL shaders → SPIR-V at build time (glslc, embedded).
- [x] Persisted pipeline cache (`$XDG_CACHE_HOME/omamovie`) — *2026-10-04*: `VulkanCompositor` loads a cache only when its Vulkan header matches the selected device, caps it at 16 MiB, and atomically replaces the disposable cache file on teardown. Cache errors fall back to an empty cache. A lavapipe run wrote a 32-byte header under a temporary `XDG_CACHE_HOME`; a second run and a run after corrupting that file both passed. On the [Iris Xe](Research/evidence/2026-10-04-m2-m3-intel-recheck.md#pipeline-cache-on-intel), the test wrote 85,175 bytes and passed again with that cache present. Startup-time benefit remains unmeasured.
- [x] CPU reference compositor (`CpuCompositor`), compared with the GPU output in tests.
- [ ] `ComputeBackend` interface for effects (`VulkanComputeBackend` + `CpuBackend`), with the first effect; non-real-time sections are reported. *Partial (2026-10-03)*: the first effects run inside both compositors instead, tested GPU against CPU: per-layer color adjustments and filter looks (`src/look.hpp`) and blur/sharpen as a reduce pass plus two separable gaussian passes into reduced RGBA16F images. The interface waits for an effect that does not fit the layer pass.
- [x] Before adding another effect-specific layer field or dispatch branch, specify how an ordered, evaluated effect description reaches the render graph and maps to the existing Vulkan and CPU passes. Keep simple effects fused in a layer pass where that is correct; add a separate pass/backend interface only for a demonstrated multi-pass need. Define stage order, color/alpha domain, time and input requirements, intermediate lifetime, bypass/mix behavior and unsupported-effect reporting. Compare CPU/GPU pixels and measure total preview/export cost, including allocations and transfers, on representative stacks; do not infer real-time behavior from an isolated shader. *Done (2026-10-07, [ADR-0016](adr/0016-video-effects.md))*: two fixed stages (detail as the spatial pre-pass, looks fused in the layer pass) with instance order inside a stage; looks compose on the CPU into the existing per-pixel `Look`, so the shaders and the CPU reference did not change and agree as before (237 compositor checks, including a new order test: black and white after sepia gives grey, sepia after black and white gives sepia). Bypass skips an instance; mix is the looks' own `amount`; unknown definitions are kept and reported, not rendered. Total preview cost of representative stacks was not re-measured: a stack adds only CPU matrix products per layer and one small vector per layer per frame.
- [x] Edge anti-aliasing for rotated layers and chroma siting from the stream (2026-10-04). CPU and Vulkan use a 2×2 coverage grid at transformed/cropped edges; the rotated-image test checks fractional alpha and CPU/GPU agreement. The probe carries FFmpeg's declared left/center/top/bottom chroma location; CPU and Vulkan bilinearly sample at that position, with center as the unspecified fallback. A color-edge fixture checks left against center and CPU against GPU. Per-frame location changes and an independent chroma-edge oracle remain unverified.

**Done when**: the S5 scenario runs on library code; GPU × CPU comparison within tolerance, independent SDR vectors and the declared supported-color contract pass. Record repeated frame times, copies and memory use; end-to-end real-time claims also require M4/S4 presentation measurements.

**Measured (2026-10-03, Iris Xe, `tests/compositor`)**: GPU vs CPU reference within 0.002
(linear, half-float output) on uploaded, VA-API and Vulkan Video frames. Three 1080p layers,
submit to completion: 3.3 ms with VA-API frames, 2.9 ms with Vulkan Video frames, 3.7 ms with
uploaded software frames — isolated means below the 16.7 ms interval. These are prior timing samples, not end-to-end playback evidence. The current test warms once and averages 20 renders; it does not record tail latency, new-frame decode or presentation. CPU/GPU agreement is not an independent color oracle.

**Chroma-siting check (2026-10-04, Iris Xe)**: after bilinear chroma reconstruction, the same 1080p three-layer test averaged 4.70 ms with GPU inputs over 20 warmed renders (submit to completion). This is a new isolated sample, not directly comparable to the earlier 3.3 ms without controlling driver/load or running paired before/after trials. The compositor test suite passed on lavapipe; no end-to-end presentation measurement was made.

**Edge anti-aliasing check (2026-10-04, Iris Xe)**: the 2×2 coverage pass averaged 4.59 ms in the same isolated three-layer test with GPU inputs. The 199 compositor checks passed on lavapipe, including fractional edge alpha and CPU/GPU agreement. Full-frame p99 and presentation remain unmeasured.

**Research**: `CLAUDE.md` §9, [premiere-pro](Research/premiere-pro.md), [davinci-resolve](Research/davinci-resolve.md).

---

### M4 — Playback and audio

- [x] PipeWire output; real-time callback without allocation or contended locks (lock-free SPSC `SampleRing`; a null output paced by a steady clock for CI and machines without PipeWire). Tested in `tests/audio`, including a producer/consumer ordering test under TSan; TSan does not prove RT deadline safety.
- [x] Master clock = audio; video drops/repeats frames. `PlaybackClock` (audible position from consumed frames minus device latency, re-anchored on seek) drives the editor's viewer: `AudioPlayer` (apps/omamovie) renders the timeline's audio on a dedicated pipeline thread into the output ring, and the video frame shown is the one at the audible sample. Silent fallback: the null output keeps an audio-paced clock. *10-minute drift (2026-10-05, [evidence](Research/evidence/2026-10-05-m4-av-drift.txt))*: one release run, 1080p30 H.264 software decode, PipeWire: first/last-minute mean error moved 2.23 ms (bound: one frame, 33.33 ms), 0 underruns, 1 dropped frame of 18,000; p99 error 33.33 ms, exactly one frame interval (the error is measured to the last composited frame, so it includes up to one frame of quantization). Not measured: presentation latency after composition. Clock steps (2026-10-06): the audible position advanced only at PipeWire callbacks (~21 ms quanta), so video moved in one- or two-frame jumps; `AudioOutput::frames_played` now interpolates between callbacks with the steady clock (capped at one quantum, lock-free seqlock from the RT callback), and the audits show composites every 16.67 ms at 1080p60.
- [x] Timeline audio mix: gain, linear fades and mute per clip, sample-accurate clip boundaries, any number of overlapping clips (`oma::audio::mix_planar`, tested); clip audio at speeds other than 1 stays silent until time-stretching exists. The mix lives in `libs/playback` (`TimelineAudio`, tested against fixtures): a clip continuing the same media where the previous one stopped (a split) keeps its decoder; per-clip three-band equalizer (`oma::audio::Equalizer`) and noise reduction (FFmpeg `afftdn` in `AudioDecoder`, noise level measured from the clip). Mono sources play as dual mono at full level. Per-block cost (2026-10-05, from an algorithmic review): `TimelineAudio::render` scanned every clip of every track and looked its media up linearly before the overlap test, O(clips × media) per 21 ms block; it now binary-searches the first clip that can sound (a transition reaches at most one neighbour past a cut) and looks media up only for clips in the block, O(log clips + clips in the block). Derived, not benchmarked; a regression test covers a transition tail two clips before the playhead.
- [x] Playback scheduler: decode ahead with bounded queues, cancellation on seek. *Done (2026-10-06)*: audio renders 0.25 s ahead into the bounded ring and restarts on seek/edit; video decodes up to 8 frames ahead in a bounded queue (`VideoScheduler`). Restarts reuse decoders (2026-10-05): `VideoScheduler` owns its `FrameSource` and `AudioPlayer` keeps its `TimelineAudio` across starts (`TimelineAudio::set_timeline` keeps the decoders of clips still present with the same media and noise reduction; tested), so play/seek/speed changes seek warm decoders instead of reopening files. Restart latency (2026-10-05, `--m4-seek-audit`, [evidence](Research/evidence/2026-10-05-m4-av-drift.txt)): 60 random seeks during 1080p30 playback held the UI thread 16 ms p50 / 24 ms p95 (30 / 39 ms when every restart reopened decoders); the first composited frame from the new position took 77–79 ms p50 and up to 145 ms, not improved by decoder reuse. The remaining block was the audio flush waiting for PipeWire's next callback (12 ms mean of ~16); the flush is now asynchronous with a write mark (`SampleRing::discard_until`, tested), and a seek holds the UI thread 2.4–3.6 ms p50, 6–9 ms p95 (2026-10-06).
- [x] J/K/L (multiple speeds), frame step. *Done (2026-10-05)*: frame step (←/→, Shift for 10) lands on exact rational frame times; J/L shuttle at 1×, 2× and 4× in both directions. Reverse (2026-10-04): stepping back past the decoded frames seeks one chunk earlier (up to 64 frames, half of a 384 MB history budget) and decodes forward to the target, keeping the chunk, so each group of pictures is decoded about once; a 1080p H.264 file with 250-frame GOPs went from 139-150 to 3.3-4.9 ms per frame played backwards (scratch benchmark through `FrameSource`, release, Iris Xe, software decode). Forward playback keeps only 8 frames of history. The history now survives restarting playback (the scheduler's decoders stay open, 2026-10-05).
- [x] On-screen compositor-image handoff for the viewer (2026-10-03): the timeline's `Composition` becomes a `RenderGraph` with every visible layer and its fit/crop/transform/opacity/blend; `PreviewItem` composites and encodes on Qt's render thread and Qt samples the compositor's own `VkImage` (no readback, no CPU conversion). The default decode remains software (one plane upload per frame, CLAUDE.md §7.3). The smoke check verifies that the viewer shows composited video; a run under the Vulkan validation layer with synchronization validation reports no errors when no window capture is taken (Qt's `grabWindow` itself triggers swapchain hazards).
- [x] Hardware decode pilot in the viewer. *Gate passed on Intel; default there (2026-10-06, [evidence](Research/evidence/2026-10-06-m4-hardware-gate.txt))*: H.264, 10-bit HEVC and AV1 at 1080p60, H.264 at 2160p30 and a 10-minute run pass with hardware decode (software fails 10-bit HEVC and, once, H.264 at 1080p60); memory stable; `OMA_PREVIEW_HARDWARE=0/1` overrides; other vendors stay on software. The gate found the audio clock advancing in PipeWire-quantum steps (~47 fps at 1080p60 on any path); interpolating it fixed playback at 60 fps. History: `OMA_PREVIEW_HARDWARE=1` lets the render thread decode with the Qt/compositor `gpu::Device`, trying VA-API → Vulkan then Vulkan Video where supported, with software fallback; thumbnails stay on the CPU path. Doing decode on the render thread avoids concurrent submissions during Qt's swapchain teardown, but also blocks presentation while decoding and bypasses `VideoScheduler`'s lookahead. On Iris Xe, smoke checks negotiated VA-API for H.264 and HEVC. An 8-second 1080p60 H.264 audit produced 372 composites and 20.67 ms p99 A/V error with hardware decode; the software route produced 371 composites and 20.00 ms p99 A/V error, with 105 scheduler drops. Both failed the 16.67 ms p99 target. Hardware-route dropped frames are not measured by the scheduler, so the audit reports them as unmeasured. Keep hardware decode opt-in until full-scene measurements show a benefit and the gate below passes. *Moved off the render thread (2026-10-05, ADR-0005)*: the pilot now decodes in `VideoScheduler` with the shared device, admitted around Qt's swapchain changes, so it gets the bounded lookahead and dropped-frame count of the software path. The Qt bridge holds the graphics queue only around `endFrame` (the Iris Xe's single queue is shared with FFmpeg's mapping). 20 s 1080p30 H.264 audit: PASS, p99 one frame, 597 composites, one drop; no validation threading/sync errors. Paused frames still decode on the render thread. The GUI smoke with hardware decode first hung: Qt's window capture emits `afterRendering` without `afterFrameEnd`, leaving the recursive queue lock held while an admitted decoder waited for it; the bridge now holds the queue with a flag released at the frame end or next frame begin (2026-10-06). Three hardware smokes, a software smoke and a 20 s hardware audit (601 composites, no drops) pass. Open: per-stage timing, the gate's longer codec/4K/seek/10-minute runs and VA-API render-node selection on hybrid systems. The FFmpeg 9.0.1 lifetime leak of its VA-API → Vulkan map (every closed hardware decoder kept its last frame, derived context and surface pool) is fixed by importing surfaces in OmaMovie (2026-10-06, ADR-0004 amendment): FFmpeg exports DRM PRIME, `gpu::DmaBufImage` imports each layer, consumers acquire/release from the foreign queue family; luma/compositor checks pass and validation reports no errors or leaked objects in the tests and a 15 s hardware run of the editor.
- [x] Adapt preview resources to detected hardware and measured playback load: choose only validated decode paths, bound frame/cache memory to available resources, and lower preview resolution when sustained missed deadlines justify it. Report the active path and fallback; allow a manual override for diagnosis. Never change timeline contents or export quality as a side effect. Compare automatic and fixed settings on the same scenes, including software fallback and device changes, before enabling automatic choices by default. *Done (2026-10-06, [evidence](Research/evidence/2026-10-06-m4-hardware-gate.txt))*: hardware decode only where the gate validated it (Intel; `OMA_PREVIEW_HARDWARE=0/1`); the frame history is bounded by min(384 MB, 1/8 of available memory); `compositor::scaled` renders a reduced preview with unchanged framing (tested), and automatic quality halves the preview after >2% dropped frames over two seconds, compared against full and fixed half on 10-bit HEVC 1080p60 and H.264 2160p60 in software (inert on the first, restores UI responsiveness on the second), so it is the default (`OMA_PREVIEW_SCALE=1` or a fraction overrides). The active decode path and every reduction are logged; the Settings screen (M6) will show them. Device changes beyond audio were not exercised.
- [x] Qt Quick editor shell through `make run-gui`: Projects/Edit, media library, viewer, responsive timeline, play/pause and seek. The storyline is the M5 `Editor` (commands and undo/redo through `Session`); the viewer evaluates the timeline at the playhead, decodes forward without reseeking during playback and follows the audio clock, fed by the bounded decode-ahead scheduler.

**Done when**: A/V drift measured over 10 minutes against the proposed predeclared pilot bound in the audit; frame presentation and device latency are accounted for, with no growing drift. Seeks, speed changes, underruns and device changes recover; UI stays responsive; TSan clean. TSan does not prove RT deadline safety.

**Status (2026-10-06, Intel)**: every M4 deliverable is in place. Against the done criterion: 10-minute drift measured with software (2.23 ms) and hardware decode (0.03 ms, no drops); device latency is in the clock (PipeWire delay + queued) and the clock now interpolates between callbacks; Qt's frame phases are measured up to `afterFrameEnd`, which includes the present submission but not scanout, so display latency after presentation remains unaccounted (Qt exposes no present timing). Seeks (UI held ~3 ms p50), speed changes, audio underruns and audio device changes recover; TSan clean. Not exercised: GPU device loss (no way to inject `VK_ERROR_DEVICE_LOST` here; render errors are logged and the viewer drops the frame, but no recovery path exists) and non-audio device changes. These two stay open in the `libs/gpu` item above; M4 is otherwise closed for Intel.

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

### M5 — Timeline model (in parallel with M2–M4) ✅ (2026-10-05, model; UI in M6/M8)

- [x] Tracks (video, audio; caption type already in the model), clips by stable media ID.
- [x] Commands with a single history transaction: insert, overwrite, append, split, trim (start/end, with or without ripple), roll, slip, slide, move, delete, ripple delete (clip and range), speed, video/audio properties, tracks, markers; user transactions are all-or-nothing.
- [x] Undo/redo; invariant `validate()`, checked by `Editor::execute` after every command (a rejected edit leaves the timeline and history untouched).
- [x] Magnetism as a **UI policy** over generic commands (`Session`: storyline cuts for reorder/drop, `snapSpan` for lane moves and trims; the commands stay generic).
- [x] Markers in the model.
- [x] Clip time mapping per ADR-0002 plus a complementary interpolation/domain ADR: constant speed first; freeze/reverse have explicit timeline duration, ramps integrate speed with explicit rounding. **Constant speed is implemented** (`TimeMap::constant`). *ADR-0013 accepted (2026-10-05)*: a map is constant or a list of linear (negative = reverse), freeze and linear-speed ramp segments covering the clip; offsets are exact rationals (ramps integrate in closed form, eased presets are several ramps), `slice` keeps edits exact, and overflow is an error. Implemented in `libs/timeline` with tests (boomerang offsets and extent, ramp integral, slice motion, rejections, overflow) and `validate()` checks coverage and the media span. Edits (2026-10-05): trims, split and ripple slice segmented maps or extend their edge segment (`TimeMap::slice/extended`); slip moves a segmented clip's media at speed 1; `set_speed` turns a segmented clip into forward playback of the media span it covered; new commands `set_time_map`, `reverse` and `freeze_frame` (one undo entry each). Playing backwards shows the frame ending at the position (one media tick before the rounded-up position), so a reversed clip shows exactly its frames in reverse. Segmented clips play no audio and offer no transition handles. Tests: reverse frames and undo, freeze hold, split/trim through a freeze, media-span rejection. Persistence (2026-10-05): native format version 2 writes a segmented clip as a `time_map` array (`linear` with `speed`, `freeze`, `ramp` with `from`/`to`, rationals as strings), keeps `speed` for constant maps, caps a map at 256 segments, and reads version 1 unchanged (identity migration, tested with a version 1 document); round trip, malformed shapes and the segment cap are tested; `oma-project dump` now writes version 2. UI (2026-10-06): a Speed submenu on every clip (Normal, ½×, 2×, 4×, a two-second freeze at the clicked point, Reverse) with a badge on retimed clips ("2×", "Reverse", "Freeze"), rippling the storyline; GUI smoke covers freeze, reverse and undo. Open: ramp presets and curve editing (M8).
- [x] Per-instant evaluation for a time `t` (`oma::timeline::evaluate` → `Composition`), see the deviation below.
- [x] Commands with no UI dependency; command serialization only when a concrete persistence/scripting requirement is established (undo alone does not require it).
- [x] Decide the connected-clip contract before ADR-0007 freezes the native DTO: an explicit anchor to a primary clip and sequence-time offset, or a documented simpler grouping policy. Specify split, trim, ripple move/delete, speed changes, locked tracks and orphan handling; make each edit one atomic undo entry. Test relative timing, invalid anchors, save/load and undo/redo. Adopt anchors only if the M6 task comparison justifies their complexity ([Final Cut research](Research/final-cut-pro.md)). *Decided and implemented (2026-10-05, ADR-0014; the maintainer chose anchors)*: a clip's optional `anchor = {primary, source}` attaches its start to a source position of a constant-speed primary on another track, so it follows the primary's content. One pass in `Editor::execute` re-attaches dependents after every edit inside the same undo entry: moves, ripples, trims and slips follow the content; a split hands the connection to the piece showing that frame; removing the primary removes the dependent; trimmed-away content or a segmented map ends the connection; moving the dependent reconnects or lets go; a follow move that would overlap fails the whole edit. `connect`/`disconnect` commands; `validate()` checks every invariant; native format 2 saves the anchor. Tests: ripple delete with undo/redo, start/ripple trims and slip, split hand-over, orphans, dependent moves, overlap rejection, connect rules, save/load. Open: the UI to connect clips and show connections, and the M6 comparison with manual grouping that decides whether anchors stay.

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

### Modular effects architecture (planned; establish before expanding the catalog)

**Source boundary before ADR-0016 (2026-10-06; replaced 2026-10-07 by the effect stack):** `VideoProperties` has one `Filter`, one
`sharpness` value and dedicated color/grade fields; `timeline::evaluate` copies those
properties, `apps/omamovie/src/viewer_frame.cpp` maps them to matching `Layer` fields,
and `libs/project/src/document.cpp` writes them separately. `EffectsDrawer` exposes
the fixed look list and soften/sharpen controls. The audio processing chain is a
separate, working audio path; do not merge video and audio processing merely to share a
type name. Existing projects and look results are the migration baseline, not a reason
to keep adding effect-specific members to `Clip` or branches in QML.

- [x] Write an effects ADR before changing the model or native DTO. Distinguish an **effect definition** (stable ID, display/category metadata, parameter schema, capabilities and compatibility), its **implementation** (the renderer operation), a project-owned **instance** (stable instance ID, order, enabled/bypass, mix, validated values and keyframes), an editable **preset** (values and optional native edit operations), and an installed **package** (identity/version, definitions, presets, assets and requirements). State ownership and module dependencies: the timeline stores plain values without Qt/GPU types; the registry resolves definitions and implementations; the project layer serializes identities/values; the app exposes semantic commands to QML. A preset does not create a new implementation. Package and catalog identity must not become the project's source of truth. *Done (2026-10-07)*: [ADR-0016](adr/0016-video-effects.md) accepted. Definitions are a built-in table in `libs/timeline`; instances are project-owned values addressed by clip and definition; presets and packages are deferred until a second source of definitions exists.
- [x] Define the smallest typed parameter and capability contract supported by two real built-in operations, including bounds/defaults, animation and time domain, color/alpha domain, required inputs, render stage and resource limits. Decide which current fields remain intrinsic clip properties (for example framing or opacity) and which become effects. Preserve their visible results and editability with a tested migration; avoid a generic node graph, per-effect subclass hierarchy or speculative GPU plugin API. Keep composition effects as ordered operations on the evaluated layer/composition; reserve a bounded scene/asset reference for future VFX scenes without implementing a scene editor. *Done (2026-10-07)*: bounded double parameters with defaults, a stage per definition, at most one instance per definition and 16 per clip; the filter and sharpness became effects through migration 3 → 4 (tested on every look and on blur/sharpen values); framing, opacity, colour adjustments, grading and titles stay intrinsic. Animation is deferred (ADR-0016).
- [ ] Add registry discovery and semantic commands for add, remove, reorder, enable/bypass, set value and apply preset as one undoable edit. Use stable IDs rather than display names or QML list positions. Evaluate keys at exact timeline/source time under the documented split/trim/speed rules, then pass plain evaluated effect data to the compositor. A preset expands to normal editable instances/commands; losing its original pack must not change pixels already defined by the project. Define duplicate/ordering rules and diagnostic behavior when a capability cannot run. *Partial (2026-10-07)*: `effect_definitions`, `with_effect`/`with_effect_moved` and `validate_effects` (timeline tests), committed through `edit::set_video` so each gesture is one undo entry; presets and keyed parameters remain open.
- [x] Prove one complete path with existing effects before growing the catalog: discover → apply → edit → undo/redo → preview → save/reopen → export → inspect decoded output. Compare the same fixture before/after migration, including effect order, bypass and missing definitions. Only then move additional clip or composition effects into the shared model. The first public release can ship a small built-in set; effect count is not a success metric. *Partial (2026-10-07)*: discover → apply → edit → undo/redo → preview → save/reopen pass (GUI smoke: a second look moved before the first and bypassed, seven undos restore the clip; project round trip keeps an unknown effect). Export (2026-10-07, [evidence](Research/evidence/2026-10-07-m7-export.txt)): a black-and-white look on one clip decodes with zero saturation there and full colour on its neighbour.

**Research**: [GPU effects](Research/gpu-effects.md), [post-production effects and catalog](Research/post-production-effects.md), [long-form effect workloads](Research/long-form-editing.md), ADR-0006 (working color), ADR-0007 (project format), ADR-0012 (grading). These sources support a staged design, not measured demand for a large catalog.

### M6 — UI

Reference design: `Docs/ui-design.md`.
UI/UX tasks and acceptance criteria: [TDDO](ui-ux-tddo.md).

- [x] Qt Quick shell: MediaPanel, PreviewPanel, TimelineView, Inspector, TransportControls. *2026-10-04*: no permanent adjustments bar (the clip's context menu opens the drawers and marks active adjustments); the library sidebar hides in any window width, leaving the viewer centered. *Done (2026-10-06)*: `Main.qml` (2,913 lines) was split without behavior changes into `ProjectsScreen`, `MediaPanel`, `PreviewPanel`, `TransportControls`, `TimelineView`, the inspector drawers (`InfoDrawer`, `VolumeDrawer`, `ColorDrawer`, `CropDrawer`, `EffectsDrawer`, with `DrawerSlider` shared) and `SettingsDialog`/`CommandPalette`; `Main.qml` keeps the window, the action registry, dialogs and layout (603 lines). Each block moved only after checking that no id inside it was used outside (the clip-menu opener moved into `TimelineView`); the GUI smoke passed after every step. The panels still read the window's ids (`root`, `actions`, `session`) through QML's context chain; making those explicit properties is the next step if `pragma ComponentBehavior: Bound` or qmllint's unqualified-access check is adopted.
- [x] MediaPanel with a **"Recordings"** source (respects `OMARCHY_SCREENRECORD_DIR`, then `$XDG_VIDEOS_DIR`/fallback; handles growing and no-audio files); thumbnails and waveforms through the job system + invalidatable cache. *Partial (2026-10-06)*: the Omarchy platform module (`apps/omamovie/src/platform/omarchy/recordings.*`) finds recordings the way the installed capture script does (`${OMARCHY_SCREENRECORD_DIR:-${XDG_VIDEOS_DIR:-$HOME/Videos}}`), newest first, and marks the capture the script names as current and still changing as growing; Projects lists them (watching the folder) with the script's preview image, "Still recording…" for a growing capture and "no audio" from a probe on the job workers; Edit starts a project with the recording on the storyline (GUI smoke, with its own recordings folder). The edit screen's library has a Recordings source with the same states and an Import button (GUI smoke). Cache (2026-10-06, ADR-0009): thumbnails (PNG) and waveforms (versioned bytes, `playback::to_bytes`/`waveform_from_bytes`, tested for round trip and damaged input) go through `DiskCache` stores under `$XDG_CACHE_HOME/omamovie/{thumbnails,waveforms}`, keyed by kind/version, size and the media identity (path, size, modification time, fingerprint hash); a background pass at startup trims each store to its 512 MB budget. GUI smoke (with its own cache folder) checks that saving and reopening leaves cached entries and thumbnails. Hit rate on reopen was not measured separately.
- [x] TimelineView: tracks, drag, trim, split, snapping, **mouse-centered** zoom, overview bar (minimap). *Partial*: storyline with ripple trim by edge drag, split, scrubbing playhead, minimap; clip dragging (storyline reorder, lane moves, library drops); snapping to clip edges/playhead with a toggle and mouse-centered Ctrl+wheel zoom (2026-10-04). Video lanes above the storyline (2026-10-06): drawn on top, nearest first, with move between lanes, trims and the clip menu; library drops above the storyline land on them; Connect (Q, also in the library's ▾ menu) places the selected item at the playhead on a free lane above (sound below) and connects it to the storyline clip there in the same undo entry (ADR-0014), shown by a stem down to the storyline; sound placed at a position connects the same way. GUI smoke covers connect, drawing and undo/redo. The storyline clip a connection follows is outlined while its connected clip is hovered or selected (GUI smoke checks both, with nothing selected for the hover case). Moving a connection point independently of the clip's start is not representable: ADR-0014 attaches the dependent's first instant, so the point moves by moving the clip (which re-attaches it); an offset anchor would be a format change for a later decision. The compact strip used above ~400 clips in view draws the same outline (2026-10-07, `Session::clipSpan`; not checked visually at that scale).
- [x] Measure timeline interaction with the [long-form workload set](Research/long-form-editing.md#proposed-gate-before-optimizations), separating duration from clip count. Profile `Session`'s full `QVariantList` rebuilds, repeated QML delegates, timeline snapshot copy and whole-model notifications. Pilot visible-range delegates or incremental updates only if they dominate the measured stalls; preserve drag, snapping and undo behavior. *Done for clip count (2026-10-06, [evidence](Research/evidence/2026-10-06-m6-long-timeline.txt))*: `--m6-timeline-audit N` measured QML delegate rebuilds as the dominant cost (C++ 2 ms vs QML ~200 ms per edit at 500 clips; 2,000 clips exhausted memory). An incremental `ClipListModel`, level of detail, a storyline model limited to the view and a compact strip above 400 clips in view bring 5,000 clips to p95 ≤ 31 ms fitted and ≤ 60 ms at a working zoom (max 60 ms), within the proposed gate. Not yet measured: gestures, lanes with thousands of clips, the two-hour source.
- [ ] Expose deliberate precise placement using the existing overwrite/lift operations; decide whether a persistent Position mode is needed by testing replacement and gap-preserving tasks against direct commands. Show which clips move or are replaced before committing a drag, including keyboard access. *Partial (2026-10-06)*: insert (W), overwrite (D), connect (Q) and lift are keyboard actions at the playhead; while a clip or library item is dragged over the storyline, the clips that will move show an arrow in their direction (GUI smoke checks it mid-drag). Open: a replacement preview for overwrite (no overwrite drag exists yet) and the Position-mode decision, which needs the M6 task comparison.
- [x] Make context menus depend on what opened them (library item, storyline clip, audio clip, cut or empty timeline area). Group related actions in submenus where useful; show only applicable actions, preserve the clicked target, and keep frequent edits directly reachable. Verify mouse, Menu key/Shift+F10, focus and undo/redo so a menu action never edits a different selected item by surprise. *Partial (2026-10-06)*: each target has its own menu: library item (add/insert/overwrite/connect), storyline clip, video lane clip (color, crop, effects, info, split, speed, delete), audio lane clip, the cut between clips (transitions) and the empty timeline (move playhead, import, fit, snapping); lane clips offer Connect/Disconnect (ADR-0014); Shift+F10/Menu opens the selected clip's menu on the storyline and on audio and video lanes. GUI smoke covers disconnect/reconnect. Keyboard check (2026-10-06, GUI smoke): Shift+F10 opens the selected storyline clip's menu with the focus inside it, Down/Enter runs its first item on that same clip, and Escape closes only the menu (the open drawer stays). *Done (2026-10-07)*: with nothing selected, Shift+F10/Menu opens the cut's transition menu when the playhead is within half a frame of a storyline cut, else the timeline's menu at the playhead; Deselect (`Ctrl+Shift+A`) makes those reachable without a mouse. The GUI smoke walks the audio lane clip's, the cut's and the empty timeline's menus (open with keyboard focus, Escape closes, Down/Enter runs the timeline menu's first item). Video lane menus share the lane code path and were not walked separately.
- [x] Contextual inspector: video (transform, crop, opacity), audio (gain, fades), text (simple title); indicator of the adjustments active on a clip. *Partial (2026-10-06)*: the drawers cover video (fit, crop, position, scale, rotation and now opacity, with keys) and audio (gain, fades, EQ, noise reduction); the clip menu marks adjusted drawers and every adjusted clip shows a dot on the timeline (GUI smoke checks opacity, the mark and undo). Text titles: decided as generated clips (ADR-0015, 2026-10-06). Done: the model (`Title` on `Clip`, validation, `ClipSource::title`, `edit::set_title`, evaluation with unlimited handles and no sound; 6 timeline tests). Native format 3 (2026-10-06): a clip's optional `title` object, text bounded before it is copied, identity migration 2 → 3; round-trip, format-1 and oversized-title tests, a title seed in the project fuzz corpus (20,000 runs). Alpha (2026-10-06): `VideoFrame::from_rgba` builds planar GBRA software frames (`SampleLayout::alpha`), and both compositors multiply coverage by the source's straight alpha, filtered with the colour's taps (shader binding 8); the CPU reference and Vulkan agree within 1e-5 on opaque, half and fully transparent columns. Rasterizer (2026-10-06): `rasterize_title` draws the text with Qt into an alpha mask at the canvas size and gives every pixel the text colour (only alpha varies, so edge filtering never darkens); each `FrameSource` keeps its last 8 titles (one owner thread each, no shared state), and the frame builder takes title layers from it on every path (paused, playback, hardware). UI (2026-10-06): Add title (`Ctrl+Alt+T`, the palette, the library's ▾ menu) puts a three-second title at the playhead on a free lane above the storyline, connected to the clip there, in one undo entry, and opens the Title drawer (text committed after a typing pause or on leaving the field, size, six picture-colour swatches, placement); a title clip's menu offers Title… besides colour, framing and effects, and moving, scaling and rotating it in the viewer use the canvas as its source. GUI smoke: add, edit (one entry each), the drawer, yellow text measured in the viewer against the same frame without the title, and undo.
- [x] Once the effects ADR and semantic commands exist, populate the Effects drawer and context menu from the registry: useful defaults first, the selected clip's ordered stack and bypass state on demand, parameter controls from the validated schema, and advanced controls only when opened. Mouse, keyboard, command palette and future automation invoke the same commands with the clicked/selected target made explicit; QML holds no effect IDs, render logic or duplicate validation. Check focus, undo/redo and narrow layouts with a real clip, including a missing effect whose settings remain inspectable. *Partial (2026-10-07)*: the drawer's look tiles come from the session's definition list and add/remove a look; the stack shows each effect in render order with bypass, amount, order arrows and removal, and lists unknown effects as unavailable (GUI smoke, screenshot checked). Keyboard (2026-10-07): look tiles are Tab-reachable buttons and Enter toggles their look (GUI smoke); the stack's controls are the shared buttons and sliders. The clip menus and the palette reach the drawer through the existing Effects action. Narrow widths (2026-10-07): at 640 px the drawer keeps two whole look tiles and the soften/sharpen slider visible while the stack shrinks and scrolls (GUI smoke measures both; screenshot checked). Per-look palette entries are deferred: the palette's Effects action opens the drawer, and a second entry point per look has no user evidence yet.
- [x] Direct manipulation in the preview (position, scale, crop) emitting commands. *Partial (2026-10-06, UX-07)*: with Crop & framing open, the selected clip shows its picture box in the viewer (from the compositor's own geometry); dragging inside moves it and the corner scales it; the picture follows through a history-free preview of the timeline snapshot, and release commits one `set_video` edit (keys honored like the sliders). GUI smoke drags the box and checks one undo entry. Crop handles (2026-10-06): one per edge at its middle; a drag maps the pointer back through the clip's source-to-canvas geometry as it was when the drag began (`Session::cropEdgeAt`, the inverse of the compositor's affine), so the edge follows the pointer in source terms, then previews history-free and commits one `setClipFraming` edit with the same rounding and 45% limit as the sliders (GUI smoke: a 30 px drag on a ~311 px box gave 0.095, one undo entry). Rotation (2026-10-06): the box turns about its center (the compositor's pivot) and a knob above it rotates the clip, Shift snapping to 15°; crop handles sit on the turned edges and map back through the rotated geometry. GUI smoke: a quarter-turn drag gives 90° ±2 in one edit, and on a clip turned 90° the left-edge handle maps back to 0 and 20% toward the center to 0.100.
- [x] Central action system (id, name, shortcut), independent of the input device; full keyboard editing. *Done (2026-10-06)*: one QML registry of `OmaAction`s (name, default keys, handler); `ActionRegistry` (app, C++) names each action after its property, registers every shortcut from that one list (the hand-kept list had missed Connect's `Q`), persists remapped keys in `$XDG_CONFIG_HOME/omamovie/shortcuts.ini` (canonical spelling, conflicts refused, reset per action or all) and searches actions for the `Ctrl+K` command palette (word prefix, substring, then subsequence, over names and ids). The palette runs the highlighted action with Enter and records a new shortcut with `Ctrl+Enter`; the GUI smoke covers search, conflicts, typing in the palette, a remapped key firing and reset. A Settings keyboard pane can reuse the registry when the Settings screen exists.
- [ ] Add a Settings entry and screen for user-facing defaults: automatic/manual preview quality and decode path, audio output, cache location/limit, recording locations, theme and export defaults. Show detected device, active path and reason for fallback; keep settings persistent, resettable and separate from project-specific choices. Disable or explain unavailable options. Verify keyboard access, restart persistence and behavior when hardware or a saved preference is unavailable. *Partial (2026-10-06)*: Settings (`Ctrl+,`, a button on Projects, the palette) keeps preview quality (automatic/full/half/quarter, applied live), decode path (automatic/hardware/software, applied at the next start with a note saying so) and the recordings folder (live) in `$XDG_CONFIG_HOME/omamovie/settings.ini`, separate from projects, with Reset all; invalid saved values fall back to defaults; environment overrides win and disable their choice. It shows the GPU, driver, the active decode path with its reason, the audio output and the font, and explains the options that do not exist yet (cache: ADR-0009; export defaults: M7; audio device: chosen in Omarchy). GUI smoke covers live application, persistence through a fresh reader, invalid values, the restart note and reset. Cache (2026-10-07): a per-store limit (256 MB, 512 MB default, 1 GB, 4 GB) applied live by an eviction pass on the job workers, and Clear cache (`DiskCache::clear`, tested) on the workers; the location stays `$XDG_CACHE_HOME/omamovie` (CLAUDE.md §15) and the screen says so. GUI smoke: the limit persists through a fresh reader, clearing empties both stores, reset restores 512 MB. Audio device (decided 2026-10-07): playback uses PipeWire's default sink, which Omarchy's audio settings choose; a second device picker in OmaMovie would disagree with the system one, so the screen points there. Export (2026-10-07): the encoder choice and the fixed output format are on the screen. Open: a run where hardware decode is chosen on a GPU that cannot do it (the gate still falls back to software, but it was not exercised).
- [x] Omarchy platform module (`apps/omamovie/src/platform/omarchy/`): theme through `omarchy-theme-color --all` + a watcher on `~/.local/state/omarchy/current/` + fallback; Omarchy font; app_id `omamovie`. *Done (2026-10-06)*: `omarchy::Theme` runs `omarchy-theme-color --all` and `omarchy-font-current` as asynchronous processes (bounded wait only at startup, killed after 2 s), watches the state directory, `theme/`, `theme.name` and `~/.config/fontconfig/fonts.conf` (re-adding watches that theme-set's delete-and-move drops), debounces 400 ms, accepts only known keys with valid colours and keeps the last good palette when a read fails or is malformed; outside Omarchy the built-in palette and the system monospace font stay. The window reports app_id `omamovie` (checked with `hyprctl clients`). The GUI smoke runs against a fake Omarchy (its own state and scripts on `PATH`) and checks a theme switch done the way theme-set does it, a malformed palette changing nothing, and a font change reaching plain text.
- [x] `omamovie.desktop`, icon, documentation of the Hyprland opacity rule. *Done (2026-10-06)*: `apps/omamovie/data/` holds the desktop entry (validated with `desktop-file-validate`; `StartupWMClass=omamovie`, opens projects and common video types), our own SVG icon and the `application/x-omamovie-project` MIME type; the PKGBUILD now builds and installs the app (Qt moved to `depends`), and the executable finds its QML relative to itself (checked by running an install-layout copy outside the repository). The README documents the opt-in opacity rule in current Omarchy's Lua syntax; it is not installed, and its effect on the window was not measured.
- [x] Projects screen (recent projects + recent recordings with "Edit") and open-by-origin rules; single instance (`ui-design.md` §3.1). *Done (2026-10-06)*: Projects lists recent project files (opened or saved, newest first, at most 12, in `$XDG_CONFIG_HOME/omamovie/recent.ini`; files that are gone are hidden, right-click removes one) above the recordings. `omamovie` with no files opens Projects; with a project file, that project; with media files, a new project with them on the storyline in the given order (imported one after another). A second launch hands its absolute paths to the running instance over a user-only local socket in `$XDG_RUNTIME_DIR` (message bounded to 64 KiB and 256 absolute paths) and exits before creating a GPU device; the window applies the same rules after the unsaved-work guard and asks to be raised (Wayland may refuse focus without an activation token). The GUI smoke checks the recent entry after save/reopen, forwarding over a private socket and the ordered import; launching twice by hand left one window that opened the file.
- [x] The Omarchy font across the UI, with live switching (`ui-design.md` §10.1). *Done (2026-10-06)*: plain Text items keep their creation font in Qt Quick, so UI text is `UiText` (bound to the live family) and Controls inherit the window's font; covered by the GUI smoke. Omarchy's `base-size` is not used: the research did not establish it as an app contract.
- [x] Initial set of our own SVG icons for v0.1 (`ui-design.md` §10.2). *Done (2026-10-06)*: 42 icons drawn for OmaMovie on the 24 px grid with a 1.5 px round stroke, kept as SVG path data in `Icon.qml` and drawn with Qt Quick Shapes (tinted by the theme, `filled` only for active states such as a key at the playhead), so no SVG module is needed; the last text-glyph placeholders (the add menu's ▾, the key toggle's ◆/◇, the will-move arrows) became icons; a misspelt icon name warns. Checked by rendering the whole set to a contact sheet and by the GUI smoke. The menus' adjusted mark (●) stays a text mark: Qt Quick menu items take icons only as image files.
- [x] OmaMovie shortcut assignments inspired by the iMovie/Final Cut convention; check action semantics, focus and compositor conflicts (`ui-design.md` §8.1). *Done (2026-10-06)*: the audit (ui-design §8.1) found Detach audio and Save As sharing `Ctrl+Shift+S`, so neither fired; Detach audio is now `Ctrl+Alt+S`. `ActionRegistry::conflicts()` lists shared shortcuts and the GUI smoke allows only the exclusive `Escape` pair; Omarchy's non-`SUPER` Hyprland binds do not collide; focus rules for fields, sliders, wheels and the palette are recorded. Screen-reader accessibility was not tested.

**Done when**: a short vlog can be edited from start to finish, including keyboard-only; the
UI event latency is measured against a predeclared interaction budget; drawer/magnetic behavior is compared with a conventional inspector/track baseline at wide/half/narrow sizes. Theme switching tolerates missing/partial files without touching the preview; Qt queue and color gates have passed.

**Research**: [imovie](Research/imovie.md), [final-cut-pro](Research/final-cut-pro.md), [movie-maker](Research/movie-maker.md), [other-editors](Research/other-editors.md), [omarchy-integration](Research/omarchy-integration.md).

---

### M7 — Project and export

- [x] ADR-0007 (project format) and ADR-0009 (cache). *ADR-0007 accepted (2026-10-04)*; [ADR-0009](adr/0009-cache.md) accepted (2026-10-06): file-per-entry store under `$XDG_CACHE_HOME/omamovie/<kind>/`, entries named by a 128-bit hash of the full key and verified against the stored key and a checksum, atomic rename, LRU eviction to 90% of a byte budget, no SQLite until a query need is measured. `oma::DiskCache` (`libs/base`) implements it (tests: round trip, torn/corrupt/colliding entries read as misses, LRU eviction, XDG location).
- [x] Versioned serialization (DTOs separate from the model), chained migrations, atomic replacement (same-filesystem temp, file sync, rename, parent-directory sync with errors surfaced), separate autosave, relink by fingerprint. *Partial (2026-10-04, `libs/project`)*: `project::Document` ↔ JSON (simdjson), `Timeline::restore` with full validation, atomic `save`, relative/absolute paths with fallback, fingerprints, newer versions refused, unknown top-level fields kept, fuzz target. App Save (`Ctrl+S`, first save asks where), Save As (`Ctrl+Shift+S`) and Open (`Ctrl+O`, Projects screen, or `omamovie file.omamovie`): saving runs on the job worker; opening re-imports the library under the saved IDs and restores the timeline; the title and Projects card show unsaved changes, and the discard guard uses them (UX-01/02). Autosave (2026-10-06): every 30 s with unsaved changes, a copy goes to `$XDG_STATE_HOME/omamovie/autosave/` (atomic save on the job worker, never over the user's file) with a `.origin` sidecar naming the project; saving with nothing changed meanwhile, starting or opening another project after the discard guard, or closing after discarding removes it. Copies left by a crash are listed on Projects ("Recovered after an unexpected close") and restore as unsaved changes to their project, or are discarded. GUI smoke (own state folder): an edit is autosaved, its own copy is not offered, a copy left as if by a crash is offered with its origin, restores with the edit as unsaved changes, and saving leaves nothing to recover; 5 consecutive passes. Save in the discard dialog (2026-10-07): Save saves first and then does what was asked (a new project asks where first); a failed or cancelled save keeps the work. Relink (2026-10-07): an opened project's file that automatic search does not find stays in the library marked missing, with Locate… on the item and in its menu; its clips play as gaps and the error says where to look. A located file must be the same kind of media and cover the range the project uses, else it is refused with the reason; once one is found, the project's other missing files are looked for (name and fingerprint) under that folder too, the project counts as changed, and the error clears when nothing is missing. GUI smoke: a renamed file moved out of reach is reported missing, a sound file is refused as its stand-in, the right file relinks it. Algorithmic note (review, 2026-10-07): each missing file runs its own `find_relocated` walk (up to 20,000 entries per root), so relinking m files costs O(m × entries) directory visits; an index of the roots by file name built once per relink would make it O(entries + m). Derived, not measured; worth doing only if projects with many missing files appear. Kill (2026-10-07, [evidence](Research/evidence/2026-10-07-m7-kill-recovery.txt)): after `kill -9` 40 s into an unsaved session, the autosave copy and its sidecar remained, valid, with the clip. Open: a kill during the autosave write and power loss (the write is atomic; not failure-injected). The first migration (1 → 2, identity, ADR-0013) exists.
- [ ] After the effects ADR, extend the native DTO with ordered effect instances, stable definition/package identities and versions, values, enabled state, mix and keyframes. Version and test migrations from the existing filter/sharpness/color/grade representation without changing old projects' output or silently discarding unsupported data. Validate sizes, counts, IDs, parameter types/ranges, asset references and compatibility at the untrusted-file boundary. Preserve unresolved instances and their values for repair; report them in the UI and `oma-project validate`, and block export (or require an explicit documented bypass choice) rather than silently rendering a different movie. Project rendering stays offline and independent of any catalog. *Partial (2026-10-07)*: format 4 stores `video.effects` (definition, enabled, numeric params), bounded at the file boundary and validated by `Timeline::restore`; migration 3 → 4 keeps old pictures; unknown definitions round-trip and `oma-project validate` warns about them; new fuzz seeds (50,000 runs clean). Open: stable package identities/versions and keyframes (deferred by ADR-0016), and blocking export on unknown effects once export exists. Export (2026-10-07) refuses to start while effects from a newer version remain, so the remaining open part is package identities/versions and keyframes, deferred by ADR-0016.
- [ ] Give project automation a stable persisted project identity/content revision and a conflict check for stale edits; keep the native DTO independent of MCP. The future agentic pilot cannot rely on `Editor::revision()` alone across reloads or processes.
- [x] Test interruption recovery and mistaken-edit recovery separately. If autosave cannot restore the latter, add a user-restorable prior project version with retention and an explicit restore path; distinguish project data from source-media backup. *Done (2026-10-07)*: interruption recovery is the autosave (GUI smoke, plus a `kill -9` [check](Research/evidence/2026-10-07-m7-kill-recovery.txt)); it cannot undo a mistake already saved, so each save first copies the file it replaces to `$XDG_STATE_HOME/omamovie/versions/<name>-<path hash>/` (the newest 10 kept; source media are never copied). A recent project's right-click menu lists them ("As it was before the save of …") and opens one as unsaved changes to that project, leaving the version in place; the menu also holds "Remove from this list", which right-click used to do at once. GUI smoke: a restored version brings back the earlier opacity as unsaved changes to the same path, and twelve saves keep ten versions.
- [x] SQLite media index, if justified in the ADR. *Not adopted (2026-10-07)*: ADR-0009 needs no query store, and project files open in ~35 ms at 5,000 clips ([evidence](Research/evidence/2026-10-07-m7-project-open-save.txt)); revisit with a measured media-search need.
- [ ] Record cold/warm project open and save by phase (read/parse, DTO/restore, reimport, UI materialization), project size and peak memory at the long-form gate. Keep JSON/simdjson unless measured project-file cost remains material after fixing avoidable copies; a SQLite media index is a separate cache decision, not an automatic project-format migration. *Partial (2026-10-07, [evidence](Research/evidence/2026-10-07-m7-project-open-save.txt))*: warm library phases measured at 500/5,000/20,000 clips: linear, 35 ms open and 22 ms save at 5,000 clips (12 MB, 107 MB peak); JSON/simdjson stays. Omitting default-valued fields would halve the file and is left until size matters. Open: cold open and the app's re-import phase.
- [x] Export: render graph → encoder (Vulkan/VA-API on Intel/AMD, NVENC on NVIDIA, software fallback) + mux, as a background job with progress and a notification (D-Bus). *Partial (2026-10-04)*: `libs/media::VideoWriter` writes BT.709 RGB24 frames to H.264/MP4 at an exact rational rate, through a temporary output replaced only on `finish()`. A three-frame NTSC-rate round trip and independent `ffprobe` count pass. Timeline rendering, audio, progress, cancellation and UI remain open. *Software path (2026-10-07)*: Export… (`Ctrl+E`) renders the sequence as it was when the export started, frame by frame through the viewer's frame builder and the Vulkan compositor on a device of the export's own (the viewer's admits other submitters only while Qt draws, ADR-0005, so a hidden window would stall it; without Vulkan, the CPU reference), reads each frame back for libx264 (the documented §7.2 exception, logged), converts linear light with a BT.709 OETF table, and mixes the sound with the playback mixer at 48 kHz, sample-accurate per frame, into an AAC track of the same `VideoWriter` (FIFO to the encoder's frame size; tested: length within one AAC frame, sine level within 0.02). It runs on the job workers with progress (an atomic polled by the UI), Cancel (the temporary output is removed) and Show file; missing media or effects from a newer version refuse to start. Output files are 0644 (they inherited mkstemp's 0600). GUI smoke: a 2 s sequence exports to an MP4 whose probed duration matches; independently, `ffprobe` reports H.264 60 frames at 30/1 tagged BT.709 and AAC 48 kHz stereo 2.005 s, frames are not black (Y 127-128) and the sound has level (-24 dB mean). Measured (2026-10-07, [evidence](Research/evidence/2026-10-07-m7-export.txt), `--export-audit`): 1080p30 went from 19.6 to ~52 fps once the GPU encodes BT.709 into RGBA8 for an 8-bit readback (the float readback and CPU conversion cost 43 ms a frame); PSNR 35.7 dB against a BT.709 source; A/V offset 0 within detection after fixing `AudioDecoder::seek`, which began past its target on AAC (sound 23 ms early in export and playback; regression tests; Opus in Matroska still 0.5 ms late). Notification (2026-10-07): when an export finishes while OmaMovie is not the active window, a freedesktop `Notify` over D-Bus (QtDBus, part of qt6-base) says so; checked against Omarchy's daemon (it returned an id), off in the smoke and audits. VA-API encoding (2026-10-07): see the encoder item below. Open: long exports (next item). *Closed for Intel/software (2026-10-07)*: NVENC waits for NVIDIA hardware in the matrix and Vulkan Video encode for driver maturity; both slot in as `VideoEncoder` kinds behind the same choice.
- [x] Define a safe automatic export encoder choice from actual device/codec capabilities and verified output, with a visible software fallback and a user-selectable override. Export settings state the output resolution, frame rate, color/audio format and quality explicitly; hardware-dependent speed choices must not silently change those output settings. *Done (2026-10-07)*: `VideoWriter` also encodes H.264 through VA-API (NV12 uploaded into the encoder's surface pool; same resolution, rate, BT.709 tags and container; tested by decoding its colour and frame count, skipped without VA-API). Automatic uses it where validated (Intel: 1080p30 export 68 fps against 52 with libx264, PSNR 36.1 against 35.7 dB, but files about twice as large) and falls back to libx264 if it cannot open, saying why; Settings offers Automatic, GPU and Software and states the fixed output (MP4, H.264 at project size and rate, BT.709, AAC 48 kHz stereo). The export notice and `--export-audit` name the encoder used. NVENC waits for NVIDIA hardware; Vulkan Video encode for driver maturity.
- [ ] For longer exports, define temporary-output and cancellation semantics, disk-full handling and an independently decoded two-hour output check at cuts, effects and beginning/middle/end A/V points. Investigate segmented/restartable export only after the continuous path is correct and interruption cost is measured. *Partial (2026-10-07)*: semantics: an export writes a temporary file next to the target (0644, removed if the export fails, is cancelled or the writer is discarded) and replaces the target only after `finish()`; Cancel stops at the next frame. Disk full is failure-injected in `tests/media` (a child limited to 64 KiB per file by `RLIMIT_FSIZE`): the writer returns an error, the previous file is untouched and no temporary remains. Two hours ([evidence](Research/evidence/2026-10-07-m7-export.txt)): a 7200 s single-clip export (320x180, VA-API, 414 fps) decodes independently with flashes and beeps exactly aligned at 10 s, 1 h and 1 h 59 min 50 s. Across a cut with out-of-order source and a dissolve (2026-10-07): markers land exactly where the time map puts them; the same run found that exporting right after opening a project rendered gaps before the library re-import had landed, now refused until every clip's media is loaded. Open: effects along a long timeline and two hours at 1080p.
- [x] `oma-project inspect | validate | dump` for the native format (2026-10-04): the CLI reuses `project::load` and `to_json`; `make test` checks valid and malformed input.
- [ ] Packaging: `omastore.toml`, GitHub release with binaries, the app installed by `package()` (the `-git` PKGBUILD and dependency declaration already exist). *Partial (2026-10-07)*: `omastore.toml` now declares the x86_64 tarball and `usr/bin/omamovie` (category and store icon unchanged); `omastore lint-manifest` passes and a local index shows it (installable once a release carries the binary). `make install` (DESTDIR, PREFIX) lays out the tree that `package()` and the release workflow both use; the workflow now attaches `omamovie-<version>-x86_64-linux.tar.gz` with its checksum. The installed binary was run from a staged tree. Open: tagging a release (outward-facing; the maintainer's call) and installing on a clean Omarchy machine.

**Done when**: save/load round trip without differences; loader fuzzing and save-failure injection pass;
exported file checked with `ffprobe`, independent decode/content inspection and measured A/V sync; installation from the PKGBUILD on a
clean Omarchy machine.

---

### Code cleanup during M6–M7

- [ ] Refactor code touched by the UI, project and export work when it improves a concrete change: clarify responsibilities and ownership, remove duplication, and keep the dependency direction in `CLAUDE.md` §5.2. Use Clean Code and Clean Architecture as guides, not as a reason to add layers or reorganize unrelated modules.
- [ ] Review public/private header boundaries and include dependencies, taking SerenityOS `AK` as an organizational reference where it fits OmaMovie. Record the conventions adopted; keep public APIs in `include/oma/<module>/` and implementation details in `src/`. Preserve behavior and confirm the affected build, tests and formatting checks after each focused change. Use an ADR for a structural change.

### M8 — Creator

- [ ] Caption track, SRT/VTT import/export.
- [ ] Changeable canvas aspect ratio (ADR: normalized coordinates vs. pixels).
- [ ] Constant speed + speed ramps with named presets (curve from ADR-0002). *Partial (2026-10-06)*: constant speeds, freeze and reverse from the clip's Speed menu (ADR-0013); ramp presets remain.
- [ ] Keyframes with interpolation on every animatable parameter; simple dopesheet. *Partial (2026-10-04)*: transform keys (`TransformKey`, hold/linear/ease, geometric scale) in source time, evaluated by `timeline::evaluate`/`transform_at`; Crop drawer "Add key" and Ken Burns. Open: other parameters (opacity, color), key markers on clips, dopesheet.
- [x] Transitions (dissolve, dip, wipe) — ahead of order (2026-10-03, ADR-0011): clip-owned, centered on the cut, limited by media handles, audio crossfade; storyline UI.
- [ ] Intent presets (PiP, split screen, cutaway) producing editable layers.
- [ ] Add built-in clip effect presets and composition presets through the common registry/command path. Composition presets may create connected layers or configure several effect instances as one undoable transaction; they remain ordinary editable project content. Keep future VFX scenes bounded to a clip/composition input, time range and asset references in this contract; pilot a scene only after a concrete editing task needs it, with no general node editor.
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

### M10 — System validation and release gates

Run after the relevant M7–M9 paths exist. Reuse `make test`, sanitizer builds and the
existing `make fuzz` targets for project JSON and `.cube` LUTs; extend them for new
untrusted inputs. Keep small deterministic regressions in CI and longer hardware runs
as recorded release checks.

- [ ] **Fuzzing and parser limits:** add coverage for the media probe and each implemented importer (including SRT/VTT if M8 ships them). Seed valid, truncated, oversized and boundary inputs; cap input size, allocation and work. Run with ASan/UBSan, retain every reproducing crash as a regression fixture, and reject invalid input cleanly. Keep the existing project/LUT corpus and exercise project migrations as versions are added.
- [ ] **Randomized edit sequences:** generate reproducible command streams across split/trim/move/ripple, speed maps, connected clips and undo/redo. After each command check timeline invariants and IDs; after undo/redo and save/reload compare the expected canonical state. Save the seed and minimized command sequence for every failure.
- [ ] **End-to-end workflow tests:** automate import → edit → save → reopen → preview → export using generated video/audio and a real Omarchy recording. Check selected frames, cuts/transitions, audio continuity and A/V sync with an independent decoder; exercise keyboard navigation and a headless/offscreen GUI smoke path where possible. Run software fallback in CI and the GPU path on actual hardware.
- [ ] **Effects contract tests:** exercise schema bounds and unknown/missing versions, ordered stack evaluation, bypass/mix/keyframe timing, undo/redo and old-project migration. Compare CPU/GPU output and independently decoded export at selected frames for a built-in clip effect and a composition preset; record pass cost and memory at 1080p and 4K on each validated device. A missing pack must leave the project recoverable and never produce a silently altered export.
- [ ] **Failure and recovery tests:** inject errors at save/autosave and export write/sync/rename boundaries, plus full disk, missing/moved media or LUT, corrupt cache, cancelled jobs and interrupted processes. Verify acknowledged project data survives, recovery is explicit, incomplete exports are not presented as finished, and workers/resources shut down without hangs. Reuse M7's save-failure and long-export checks rather than duplicating them.
- [ ] **Concurrency and lifetime stress:** loop play/pause/seek, project switches, viewer resize, decode-path fallback, export cancellation and shutdown while decode/compositor/audio jobs run. Check bounded queues, deadlock timeouts and device-resource lifetime under TSan/ASan and Vulkan validation layers; test device loss where injection is supported. Record which cases ran on Intel, AMD, NVIDIA and hybrid hardware.
- [ ] **Sustained workload and regression measurements:** run the [long-form gate](Research/long-form-editing.md#proposed-gate-before-optimizations) plus repeated open/edit/preview/export cycles. Record frame drops, A/V drift, p50/p95/p99 and worst interaction/seek times, peak and end-of-run RAM/VRAM, file descriptors, cache/disk use and export correctness. Compare against a committed baseline for the same machine, driver and media; investigate growth or regressions before release.

**Done when**: deterministic and sanitizer suites pass in CI; bounded fuzz campaigns
pass for every shipped untrusted parser with no unresolved crash; the edit-sequence,
workflow, recovery and concurrency checks pass on their declared paths; long-form
results and hardware coverage are recorded with fixtures, seeds, commands and versions.
Unrun hardware paths remain explicitly unvalidated, and a failing release gate is fixed
or has its supported claim narrowed before v0.1.

---

### Conditional pilot — Community effect packs and curated marketplace (after M7/M8)

The maintainer requested a place for others to publish production/post-production
effects (2026-10-04). This is an explicit future scope direction under
`CLAUDE.md` §2; the public extension architecture still needs an ADR. An installed
package and a curated catalog are separate: the core registry reads local packages,
while an optional catalog only helps people find and install them. Editing, preview,
save/reopen and export work offline, without an account or marketplace service.
Start with community **data-only packs** of presets built from native operations,
so applying one creates editable project content without loading external code.

- [ ] After the effects ADR, define a versioned package manifest with stable package and preset IDs, author/license, contents, preview, compatible OmaMovie and effect-definition ranges, asset hashes and bounded file sizes. Keep package installation separate from project instances: the project stores the identities and values needed to render or explain a missing dependency. Pilot local install/update/disable, project save/load and missing-pack recovery before adding a catalog. Show source, version and contents before installation; preserve a working prior version if an update fails.
- [ ] Compare installing a pack with manually reproducing the same effect on target tasks. Measure discovery, completion, corrections and export fidelity; expand the catalog only if it improves that workflow.
- [ ] Only after data-only packs and the core contract prove useful, write a separate executable-host ADR and prototype one isolated out-of-process effect before accepting code submissions. Specify parameters, color/alpha/time, threading, bounded resources, deterministic export, version pinning, crash/timeout behavior, permissions, update rollback and missing-effect reporting. Measure CPU↔GPU transfer and p95/p99 preview time; prove any Vulkan handoff separately. Do not load downloaded native code into the editor by default. Evaluate OpenFX against this contract rather than promising universal plugin compatibility.
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
| Color space + libplacebo | M1 | 0006 | **Accepted**: linear BT.709 RGBA16F, sRGB display; own shaders, libplacebo deferred |
| Project format | M7 | 0007 | **Accepted**: versioned pretty JSON, simdjson, `Timeline::restore` |
| ProjectIR and preservation | M9 | 0008 | `CLAUDE.md` §16 |
| Cache | M7 | 0009 ✅ | Key derived from inputs, `$XDG_CACHE_HOME/omamovie` |
| Canvas coordinates | M8 | 0010 | Decide before the aspect ratio becomes changeable |
| Transitions | ahead of M8 | 0011 | **Accepted**: clip-owned, centered on the cut, media handles |
| Clip time maps (freeze, reverse, ramps) | M5 | 0013 | **Accepted**: constant or segments (linear, freeze, linear-speed ramp), exact |
| Connected clips | M5 | 0014 | **Accepted**: anchor to a primary's source position, re-attached after every edit |
| Modular effects and project instances | before expansion in M7/M8 | new ADR | Definitions, implementations, instances, presets and packages separated; ordered evaluated stack, migration and missing-effect policy to be decided |
| Executable third-party effect host | conditional pilot | later ADR | Isolated process and compatibility/resource contract; contingent on the data-only pack pilot |

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
| M8 effects | Registry/command contract; editable stack and presets; CPU/GPU frames, saved-project and decoded-export checks |
| M9 | Fuzzing of every importer; small fixtures per compatibility level |
| M10 | Randomized edits; workflow and recovery checks; concurrency and sustained stress; reproducible fuzzing and release evidence |

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

M5 is closed for the model and M4 for Intel (see their status notes). Next, in order:
1. **M6 UI**: the shell's open items, the connected-clip and time-map UI (freeze, reverse,
   ramps, connections), context menus, the Settings screen (which will show the active decode
   path and preview quality), Omarchy integration, and the measured interaction/usability gates.
2. Open hardware items: device loss and display latency after presentation (M2/M4), NVIDIA and
   AMD paths (S7/S8), hybrid laptops.
3. Per-frame allocations on the viewer path (CLAUDE.md §23): `ViewerFrame`, the render graph's
   layers and curve points, and `timeline::evaluate` copying each layer's `VideoProperties` are
   allocated every frame. The gate shows composition at 3.5 ms p50 at 1080p with hardware
   decode, so they are not the bottleneck; revisit when a profile shows allocator time.

In parallel, M7's project format (ADR-0007) can start: the timeline model and the media library
are stable enough to persist. Resolve **S6/ADR-0006** before any color claim beyond the sRGB
preview transform. Schedule S7/S8 on real hardware before declaring AMD/NVIDIA paths validated.
