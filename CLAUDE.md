# CLAUDE.md — OmaMovie

Operational guide for coding agents working in this repository. It defines engineering
rules, architectural invariants, priorities and conventions. It is not user documentation.

> **Simple UI, serious pipeline.**
> The UI may hide complexity. The architecture may not pretend it does not exist.

---

## 0. Repository status

- **M0 done** (2026-10-02): GNU Make build, `libs/base` (rational time, `Result`/`Error`,
  logging, job system), tests with Cest, CI and ADRs 0001–0003. The other libs, the app
  and the tools do not exist yet.
- The structure, commands and decisions below are the **target**. When creating something
  this document describes, follow it. When diverging, record why in an ADR (§20) and
  update this file in the same commit.
- Keep §3 (Commands) in sync with reality. A documented command that does not work is a bug.
- What to build and in which order: `Docs/implementation-plan.md`. The research behind the
  decisions lives in `Docs/Research/`.
- **All repository content is written in English**: docs, READMEs, ADRs, code comments,
  log messages, CI step names and commit messages.

---

## 1. Identity and naming

| Item | Name |
|---|---|
| Product | `OmaMovie` |
| Repository | `oma-movie` |
| Main executable | `omamovie` |
| Reusable libraries | `oma-media`, `oma-timeline`, `oma-compositor`, `oma-project`, … |
| Project CLI | `oma-project` |
| Root C++ namespace | `oma::` (one sub-namespace per module: `oma::media`, `oma::timeline`, …) |

**Forbidden:** `OmaVideo`, `omavideo`, `oma-video` anywhere (code, docs, file names, log messages).

Target platform: **Omarchy** (Arch Linux + Hyprland, Wayland). The project is **not
cross-platform**. Do not add portability layers for Windows/macOS, nor X11 paths. It is fine
to assume a recent toolchain (current Arch GCC/Clang), recent Qt 6, recent Mesa/NVIDIA,
PipeWire and Wayland.

---

## 2. Scope

### Goals
Non-destructive editing · multitrack timeline · GPU compositing (Vulkan) · real-time
playback · GPU-first pipeline · modern formats · importing projects from other NLEs ·
modular architecture · Omarchy integration · simple, contextual UI · growth without
rewriting the core.

### Non-goals (until an explicit decision says otherwise)
- a Premiere clone or an After Effects–style compositor;
- full motion graphics, advanced multicam, complex tracking;
- AI without a concrete use case;
- cloud, remote collaboration;
- **a third-party plugin system** before the architecture stabilizes;
- writing proprietary third-party formats before the parser is well understood.

If a task pushes toward a non-goal, stop and ask.

---

## 3. Commands

GNU Make build (ADR-0001). `make help` lists everything.

```sh
make deps                      # install every dependency declared in the PKGBUILD (sudo pacman)
make -j                        # libs + tests + compile_commands.json (BUILD=debug)
make -j test                   # build and run the tests (generates the fixtures first)
make -j test FILTER=rescale    # only tests whose name contains the pattern
make -j BUILD=asan test        # AddressSanitizer + UBSan (third-party suppressions: tests/support/)
make -j BUILD=tsan test        # ThreadSanitizer
make -j BUILD=release test     # -O2
make -j CXX=clang++ test       # another compiler (CI runs g++ and clang++)
make format | make format-check
make tidy                      # clang-tidy on the libs (warnings are errors)
make fixtures                  # regenerate test media in tests/fixtures/generated/ (ffmpeg)
make spikes                    # M1 spikes in tools/spikes (Docs/spikes/)
make run-gui                   # Qt/Vulkan editor shell; GUI_FILE=path or RUN_GUI_SMOKE=1
makepkg -si                    # build and install the Arch package from the PKGBUILD
```

Commits and releases:

```sh
scripts/check-commits.sh origin/master..HEAD   # validate Conventional Commits before pushing
git tag v0.1.0 && git push origin v0.1.0       # triggers the release workflow
```

- **Conventional Commits in English**, imperative mood, no trailing period, at most 100
  characters: `<type>(<scope>): <description>`. Types: `feat fix docs style refactor perf test
  build ci chore revert`. Scope = lib or area (`base`, `media`, `timeline`, `ui`, `research`,
  `adr`...). `!` marks a breaking change.
- **Microcommits**: one commit per coherent change that builds and passes the tests on its
  own. Do not mix refactoring with features, nor docs with code, in one commit.
- CI validates commit messages (`.github/workflows/commits.yml`), builds and tests with
  g++/clang++ × debug/asan/tsan/release (`ci.yml`). `vX.Y.Z` tags produce a release with
  notes built from the commits (`release.yml`, `scripts/changelog.sh`).

Target commands, not available yet:

```sh
# Run
./build/debug/apps/omamovie/omamovie

# Project CLI
./build/debug/tools/oma-project/oma-project inspect <file>

# Benchmarks
./build/release/tools/bench/oma-bench --scenario <name> --json out.json
```

Before declaring a task done: `make test` passes without new warnings, and concurrent
changes also pass with `BUILD=tsan`. If you did not run the tests, say so.

---

## 4. Stack

| Layer | Technology | Notes |
|---|---|---|
| Core | C++23 | `std::expected`, `std::span`, concepts. Arch compilers support it. |
| Build | Non-recursive GNU Make (`Makefile` + one `module.mk` per lib) | ADR-0001. **Do not use CMake.** |
| Tests | Cest (`third_party/cest`, header-only) | ADR-0001; rules in §25. |
| UI | Qt 6 Quick / QML | Presentation and interaction only. |
| Media | FFmpeg (libavformat/codec/util/filter/swresample) | Wrapped by `oma-media`. |
| GPU / compositing | Vulkan | Main graphics backend. |
| Compute | Vulkan Compute, CUDA, CPU | Behind `ComputeBackend`. **No OpenCL** (§9.3). |
| Audio | PipeWire | System integration. **Do not use OpenAL** as the foundation. |
| Structured data | SQLite | Only where justified (§17). |
| Crypto | OpenSSL | Only for real cryptographic needs. Content hashing alone does not require OpenSSL. |

**Dependencies are declared in the `PKGBUILD`** (single source of truth): `depends` for what
shipped code uses at runtime, `makedepends`/`checkdepends` for building and testing the
package, and `_devdepends` for development-only tools. `scripts/deps.sh` and CI install from
it; never list packages anywhere else (README, CI files, scripts). Move a package from
`_devdepends` to `depends` when shipped code starts using it.

**New dependencies:** any dependency not listed here needs a justification in the commit/PR
(problem solved, alternatives considered, build/runtime cost) and an entry in the `PKGBUILD`.
A large dependency needs an ADR. Do not add a framework to solve a trivial problem.

Fuzzing with libFuzzer (Clang).

---

## 5. Module architecture

### 5.1 Layout

```
oma-movie/
  apps/
    omamovie/            # Qt Quick executable. Wires everything together. QML + C++ bridge.
  libs/
    base/                # Result/Error, logging, time (RationalTime), utilities. No Qt.
    gpu/                 # Vulkan device, memory, sync, interop (DMA-BUF, CUDA). No Qt.
    media/               # oma-media: probe, demux, decode, encode, mux, frames, hwaccel.
    audio/               # Mixer, resampling, clock, PipeWire output.
    compositor/          # Render graph + Vulkan compositor + effects.
    timeline/            # Timeline model, editing operations, undo/redo.
    project/             # Native format, serialization, migration.
    project-ir/          # ProjectIR: neutral intermediate representation.
    importers/           # One subdirectory per external format -> ProjectIR.
  tools/
    oma-project/         # CLI: inspect, dump, diff, validate, convert.
    bench/               # Pipeline benchmarks.
  tests/
    <lib>/               # Cest tests for each lib (module.mk + main.cpp + test_*.cpp).
    support/             # oma_test.hpp: the Cest entry point for tests.
    fixtures/            # Small, reproducible fixtures + generator scripts.
  third_party/           # Vendored, pinned third-party code (cest/).
  scripts/               # Repository scripts (commit check, changelog).
  Makefile               # Build; each lib has libs/<lib>/module.mk.
  PKGBUILD               # Arch package and the single source of truth for dependencies.
  Docs/
    adr/                 # Architecture Decision Records.
    Research/            # Product/UX and technology research (non-normative).
```

`libs/base` and `libs/gpu` were not in the original sketch: `base` keeps time, errors and
logging from being duplicated or coupled to Qt; `gpu` lets the compositor, compute and decode
interop share one `VkDevice` without `media` depending on `compositor`. The structure may
change with a recorded technical justification.

### 5.2 Dependency rules (invariants)

```
base  <-  gpu  <-  media  <-  compositor
base  <-  audio
base  <-  timeline           (timeline does NOT depend on gpu, concrete media, compositor or Qt)
base  <-  project-ir         (project-ir does NOT depend on timeline, project or importers)
project-ir <- importers      (importers do NOT depend on OmaMovie's internal model)
timeline, project-ir <- project
everything  <-  apps/omamovie, tools/*
```

- The Makefile enforces this graph (`ALLOWED_DEPS_<lib>`): a forbidden dependency fails the build.
- **Qt only in `apps/` and in explicitly marked bridge code.** The libs (`base`, `gpu`,
  `media`, `timeline`, `project-ir`, `project`, `importers`) do not include Qt headers. This
  keeps `oma-media` separable and testable without a UI. Possible exception: the Qt RHI ↔
  Vulkan integration, which lives in the app (§9.4).
- **No Vulkan, CUDA or FFmpeg includes in the public headers of `timeline`, `project`,
  `project-ir` or `importers`.**
- FFmpeg headers (`libav*`) appear only inside `libs/media` (and in `tools/` if needed for
  diagnostics).
- Dependency cycles between libs are forbidden. If one appears, the design is wrong.
- Each lib exposes its public API in `include/oma/<module>/` and hides its implementation in `src/`.

---

## 6. Time representation (critical invariant)

**Never use `float`/`double` as the primary time representation.** Doubles are allowed only
for display in the UI and for disposable derived values.

Types in `libs/base` (implemented, ADR-0002):

- `Rational` — `num/den` in `int64`, always normalized, `den > 0`.
- `RationalTime` — an instant: `value * timebase` seconds.
- `TimeRange` — half-open interval `[start, start + duration)`.
- `FrameRate` (rational: 30000/1001, not 29.97) and `SampleRate` as distinct types.

Rules:
1. **Conversions are explicit** and specify rounding (`Floor`, `Ceil`, `Nearest`). No
   implicit conversion between timebases.
2. Rescale with 128-bit arithmetic (`__int128`); detect overflow, never truncate silently.
3. Compare times in different timebases only through the exact comparison.
4. **Do not assume CFR.** Frames are located by PTS, never by `index * frame_duration` in
   arbitrary media. VFR is the normal case.
5. Audio is addressed in samples (sample-accurate). Audio editing must not be quantized to
   the video frame grid.
6. The timeline has its own timebase (project/sequence). Each clip maps timeline time →
   media time through an explicit `TimeMap` (including speed and speed ramps, ADR-0002).
7. Timestamps from FFmpeg (`AVRational`, `AV_NOPTS_VALUE`) are converted at the `oma-media`
   boundary; `AV_NOPTS_VALUE` becomes an empty `std::optional`, not a magic number.

Time conversion tests are mandatory and must cover: 23.976/29.97/59.94, 44.1k/48k/96k, large
values (hours of media in a 90 kHz timebase), rounding and overflow.

---

## 7. GPU-first pipeline

The GPU compositor is part of the fundamental architecture, not a future optimization.

### 7.1 Target path

```
compressed media -> demux -> HW decode -> GPU surface -> Vulkan compositor -> effects -> preview
                                                                         \-> HW encode (export)
```

### 7.2 Forbidden anti-pattern in playback/export paths

```
CPU decode -> upload -> process -> download -> upload again
```

Any GPU→CPU readback on the hot path needs an explicit justification in a comment and must be
visible in profiling and in the `gpu` log category.

### 7.3 Decode per vendor (investigate and validate before committing)

| Hardware | Preferred path | Vulkan interop |
|---|---|---|
| Intel / AMD | VA-API through FFmpeg hwaccel, or Vulkan Video | DMA-BUF export → Vulkan import (`VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`) |
| NVIDIA | NVDEC (FFmpeg `cuda` hwaccel), or Vulkan Video | CUDA ↔ Vulkan external memory/semaphores (`VK_KHR_external_memory_fd`, `cuImportExternalMemory`) |
| Any | FFmpeg Vulkan hwaccel (Vulkan Video decode) | Native, if the driver supports the codec |
| Fallback | Software decode | A single upload to the GPU through a reused staging buffer |

- Detect capabilities at runtime (codec × profile × resolution × bit depth × driver) and log
  the chosen path and why.
- A software fallback is mandatory and must work; the HW path is not the only path.
- Encode: VA-API, NVENC, Vulkan Video encode when mature; software fallback (e.g.
  libx264/libx265/SVT-AV1).

### 7.4 Color

- YUV→RGB conversion in the shader, honoring the stream's matrix, range (limited/full),
  primaries and transfer. Never assume BT.709 limited without reading the metadata.
- Support 10-bit (P010 and similar) from the frame format design onward.
- The compositor's working space (e.g. linear float16) is an ADR decision. Blending happens
  in the space defined there, not "wherever it works".

---

## 8. oma-media

A reusable layer, separable from the app. Responsible for: probing, demux, decode, encode,
mux, GPU/CPU frame abstraction, frame lifetime and synchronization, hardware acceleration,
conversion.

Rules:
- **No FFmpeg calls outside `libs/media`.**
- Abstractions must have architectural value (lifetime, threading, backend selection, strong
  types). Do **not** write one-line wrappers that only rename FFmpeg functions. Inside
  `libs/media`, using the FFmpeg API directly is fine.
- FFmpeg resources are RAII: `unique_ptr` with deleters for `AVFormatContext`,
  `AVCodecContext`, `AVFrame`, `AVPacket`, `AVBufferRef`.
- FFmpeg errors are converted to `oma::Error`, preserving the code and `av_strerror`.
- Probing returns its own structure (streams, codec, dimensions, timebase, duration, color,
  rotation/display matrix, channel layout), not `AVStream*`.

### 8.1 Frames and lifetime

- `Frame` is a handle with **explicit ownership** and controlled reference counting (pool),
  not `shared_ptr` scattered around. Releasing returns it to the pool.
- A GPU frame carries: Vulkan image(s) / imported memory, format, color space, PTS
  (`RationalTime`) and a **synchronization primitive** (Vulkan timeline semaphore) that tells
  when it is ready and when it can be reused.
- No frame may outlive the device/pool that created it. Shutdown destroys in order:
  consumers → pools → device.
- No `vkDeviceWaitIdle`/`vkQueueWaitIdle` on the hot path. Synchronize with specific
  semaphores/fences.
- HW-decoded frames keep a reference to the source `AVFrame`/surface while the imported image
  is in use.

---

## 9. Compositor, render graph and backends

### 9.1 Layers (invariant)

```
Timeline (model)  ->  RenderGraph (description per instant)  ->  Compositor  ->  Graphics backend (Vulkan)
```

- The timeline **does not know Vulkan**. For a time `t` it produces a declarative description
  (layers, transforms, crop, opacity, blend, effects with evaluated parameters).
- The render graph is plain data and testable without a GPU ("which graph comes out of this
  timeline state").
- The compositor translates the graph into backend commands and manages pipelines,
  descriptor sets and transient resources.

### 9.2 Compositor responsibilities
Layers, scale, crop, rotation, translation, alpha blending, masks, transitions, LUTs, color
transforms, overlays, titles, blend modes.

### 9.3 Compute

```cpp
class ComputeBackend;      // interface
// VulkanComputeBackend, CudaBackend, CpuBackend
```

- **Effects do not depend directly on CUDA.** An effect declares what it needs; the backend
  executes it.
- `CpuBackend` exists as the correctness reference and for tests (output comparison with a
  tolerance).
- **Vulkan Compute is the generic backend**: every effect has a Vulkan implementation.
- CUDA only where there is a measured gain on NVIDIA.
- **OpenCL is not used.** Reasons: Omarchy installs no OpenCL runtime, and rusticl (Mesa) has
  no zero-copy interop with Vulkan, which would force frame copies (violates §7). See
  `Docs/Research/opencl.md`. Do not add OpenCL without an explicit decision and an ADR.
- Do not implement the three backends at once. Interface first, one real backend, CPU for
  tests; the others when there is a reason.

### 9.4 Preview in Qt Quick
- Qt Quick must run on the **Vulkan** RHI backend, sharing or importing compositor images
  without a CPU readback.
- Investigate and record in an ADR: a shared device (Qt using OmaMovie's `VkDevice`) vs.
  separate devices with external memory. Prefer what avoids copies and simplifies
  synchronization.
- The QML preview item only displays the final texture. It does not decide what to render.

### 9.5 Pipelines and shaders
- Shaders in GLSL or Slang compiled to SPIR-V at build time. Persisted pipeline cache (an
  invalidatable cache, §15).
- No pipeline creation during playback if it can be done earlier (warm-up).

---

## 10. Timeline

The central component. Progressive support for: multiple video and audio tracks, clips,
in/out, split, trim, ripple, roll, slip, slide, snapping, transitions, keyframes, speed,
nested structures (when justified), undo/redo.

Rules:
- Timeline model in plain C++ (`libs/timeline`), without Qt, GPU or FFmpeg.
- **Every edit is a command** (`Command` with `apply`/`revert`), applied through a single
  entry point. Undo/redo is born with the model, not added later.
- Compound operations (e.g. ripple delete) form a single transaction in the history.
- Invariants checkable by a function (`validate()`) and tested: no improper overlap in a
  track, valid ranges, existing media references.
- Editing is non-destructive: clips reference media and ranges and never modify source files.
- Clips reference media by stable ID, not by pointer or path.
- **Render state never lives in the timeline's QML components.** The UI reads an exposed
  model (`QAbstractItemModel`/properties) and emits edit intents.

---

## 11. UI (Qt Quick / QML)

Inspired by the simplicity of iMovie, without copying its visual identity or proprietary
behavior. **Reference design and decisions: `Docs/ui-design.md`** (layout, adjustment drawer,
timeline, shortcuts, theme). Diverging from it requires updating that document.

Fixed decisions (2026-10-02):
- Adjustment controls in a **drawer above the viewer**, not in a fixed side panel.
- **The Omarchy font** across the whole UI (`omarchy-font-current`, with a fallback).
- **Our own SVG icons**, monochrome and tinted by the theme. No Nerd Font glyphs and no
  third-party icons without a compatible license.
- Shortcuts follow the **iMovie/Final Cut convention** (`Cmd`→`Ctrl`); never use `SUPER`.
- The start screen **depends on how the app was opened**: launcher → Projects; project or
  media file → Edit. Single instance.

Structure: `MediaPanel`, `PreviewPanel`, `TimelineView`, `Inspector`, `TransportControls`.

- Large preview, clear timeline, visual library, few permanent controls.
- **Contextual inspector**: the selection determines its content.
  - Video: transform, crop, opacity, speed, color, effects.
  - Audio: gain, fades, EQ, processing.
  - Text: font, size, layout, animation.
- Advanced tools appear in context, not as always-visible panels. Hiding complexity ≠
  removing capability.
- Visual integration with the active Omarchy theme (mechanism documented in
  `Docs/Research/omarchy-integration.md`); isolate that reading in a single place. The viewer,
  thumbnails and scopes **never** take colors from the theme.
- Native Wayland. Do not use X11 APIs.

### 11.1 QML vs C++
QML: layout, interaction, UI animation, property bindings.
**Forbidden in JavaScript/QML:** decoding, a complex timeline model, the render graph,
compositing, synchronization, persistence, project parsing. Non-trivial JS logic inside QML
means a C++ model is missing.

### 11.2 Shortcuts
Full keyboard editing is a requirement. Plan early for: split, delete, ripple delete,
undo/redo, frame step (±1 frame, ±N frames), J/K/L (including multiple speeds by repeated
presses), in/out, next/previous clip selection.
- Actions are registered in a central action system (id, name, default shortcut, handler),
  not in scattered `Keys.onPressed`.
- Shortcuts must be remappable later; do not hardcode them in components.

---

## 12. Audio

- PipeWire is the main integration. Do not use OpenAL as the foundation.
- **During playback the master clock is audio** (derived from the samples actually consumed
  by the device). Video syncs to it: frames are dropped or repeated; audio is never
  "stretched". Without audio, use a monotonic clock.
- The PipeWire real-time callback **does not allocate, block, log synchronously or take a
  contended mutex**. Communication through preallocated lock-free queues/ring buffers.
- Cover: project vs. media vs. device sample rate, resampling (libswresample or equivalent,
  in `media`/`audio`), volume, fades, mixing, waveforms, device-reported latency.
- Internal processing in planar float32 (to be confirmed in an ADR).

---

## 13. Concurrency

Never block the UI thread with: decode, encode, thumbnails, waveforms, probing, cache,
proxies, imports, large saves.

Thread model (ADR-0003):

| Thread / pool | Responsibility |
|---|---|
| UI (main) | QML, events, edit command dispatch |
| Render (Qt) | Scene graph, preview presentation |
| Playback | Clock, frame scheduling, requests to decode/compositor |
| Decode workers | One pipeline per active stream, with a bounded queue |
| Audio RT | PipeWire callback (rules in §12) |
| Job pool | Background tasks: probe, thumbnails, waveforms, proxies, imports, export |

Rules:
- **No ad hoc `std::thread` in components.** Every asynchronous task goes through the job
  system (`libs/base`).
- Every long task supports **cancellation** (cooperative token) and **progress**.
- Ownership of data passed between threads is explicit (move, pool handle, or shared
  immutable). Document which.
- Queues between stages are **bounded** (backpressure). No queues that grow without limit.
- Safe shutdown: cancel jobs → drain queues → stop workers → release the GPU. Test shutdown
  in the middle of playback and of an import.
- Callbacks to the UI arrive on the UI thread through a single mechanism (e.g.
  `QMetaObject::invokeMethod` with `Qt::QueuedConnection` in the bridge).
- Before changing threading, describe the impact on lifetime and synchronization (§21).

---

## 14. Native project format

- Our own format, **versioned** (integer `format_version` at the top).
- The persisted format does **not** mirror in-memory structs. There is an explicit
  serialization layer (DTOs/schema) between the model and the disk.
- **Explicit chained migrations** `vN -> vN+1`, each tested with a fixture of the old
  version. Never edit a published migration.
- Opening a project newer than supported: refuse with a clear message (or read-only mode),
  never corrupt it.
- Atomic save: write to a temporary file → `fsync` → `rename`. Backup/autosave separate from
  the main file.
- Must represent: media references, timelines, tracks, clips, transitions, effects,
  keyframes, metadata, and unsupported external objects (§16.3).
- A media reference stores: path relative to the project, absolute path and a fingerprint
  (size + partial hash + duration/streams) for relinking.
- Unknown fields from compatible future versions are preserved, not dropped.
- The concrete format (JSON / single file / bundle with SQLite, etc.) is an ADR decision.
  Requirements: diffable when possible, robust against truncated files.

---

## 15. Cache

Design the cache from the start, even if only partially implemented.

Kinds: decoded frames, thumbnails, waveforms, proxies, rendered intermediate frames, shader pipelines.

- **The cache is never a source of truth.** Deleting the whole cache loses no project data.
- Every entry has a key derived from the inputs that produced it (media identity +
  parameters + algorithm version). If an input changes, the key changes: invalidation by
  construction.
- Configurable memory/disk limits with an eviction policy (LRU or similar).
- Disk caches live in `$XDG_CACHE_HOME/omamovie/`, never next to the project unless
  explicitly requested.
- The GPU frame cache respects the VRAM budget and the synchronization in §8.1.

---

## 16. Interoperability and reverse engineering

### 16.1 Three distinct categories (do not mix)
1. **Media formats** (containers/codecs) → `oma-media`.
2. **Interchange formats** (OTIO, EDL, AAF, FCPXML/XML) → `importers/`.
3. **Proprietary projects** (Premiere, Resolve, Avid, Vegas, …) → `importers/`.

### 16.2 ProjectIR

```
External project -> <Format>Importer -> ProjectIR -> converter -> OmaMovie project
```

- `ProjectIR` is neutral, independent of any external format and of the internal model.
- Importers produce **only** ProjectIR. They do not touch the internal model.
- **Details of external formats never leak into the core model.** If the internal model needs
  a new concept, it is added on its own merit, with its own name.
- Times in ProjectIR use `RationalTime`, preserving the source timebase.

### 16.3 Preserving external data
Never drop silently. Represent as `UnknownEffect`, `UnsupportedEffect`,
`OpaqueExternalObject`, storing: source format, original identifier, raw payload (when
feasible) and partially parsed parameters. The goal is to allow a future round trip.

### 16.4 Import report
Every import produces a structured report (shown in the UI and the CLI) with, for each issue:
affected object, reason, compatibility level reached, likely impact (e.g. "transition replaced
by a hard cut; duration preserved").

### 16.5 Support levels
Never write "supports Premiere". Declare per format **and version**:

| Level | Meaning |
|---|---|
| 0 | File recognized and version identified |
| 1 | Referenced media and clips |
| 2 | Tracks, cuts, transitions |
| 3 | Transforms, audio (gain/fades), keyframes |
| 4 | Effects with a mapped equivalent |
| 5 | Round trip |

Keep a table in `Docs/interop.md` with the level per format/version and the tests that prove
each level.

### 16.6 Reverse engineering rules
- Goal: **interoperability**, focused on project representation.
- Start **read-only**. Writing only after the parser is understood and tested.
- **Forbidden:** implementing anything meant to bypass DRM, licensing, protective encryption,
  authentication or access control. If a format is protected, document it and stop.
- Priority order by accessibility: text → XML → JSON → ZIP/container → SQLite → known
  structured binary → proprietary binary → protected. Validate the architecture with
  accessible formats (OTIO, FCPXML, EDL, export XMLs) before attacking hard binaries.
- Document format findings in `Docs/formats/<format>.md` (structure, observed versions,
  known/unknown fields, how each was deduced).

### 16.7 `oma-project` CLI
`inspect` (summary and detected level) · `dump` (structured tree/ProjectIR) · `diff` (compare
two projects with small known changes, field/offset-oriented output, for RE) · `validate`
(invariants) · `convert` (through ProjectIR). The CLI reuses the same libs as the app. No
parsing logic exists only in the CLI.

---

## 17. Persistence and SQLite

SQLite for: media metadata, indexes, structured caches, project metadata when it makes sense.
- Do not use SQLite "because it is there". Justify it in the module.
- Large binaries (proxies, frames, large waveforms) stay as **files**; the database stores
  references.
- Versioned schema with migrations, like the project format.
- Database access off the UI thread.

---

## 18. Parser safety

Media and project files are **untrusted input**, always.

- Validate sizes, offsets and counts before use. Size arithmetic with overflow checks.
- Limit allocations (cap per structure and in total); never allocate from an unbounded size
  read from the file.
- Truncated files and invalid structures produce a handled error, never a crash, UB or an
  infinite loop.
- Limit recursion depth in nested formats.
- XML parsers: disable external entities (XXE) and entity expansion.
- ZIP: protect against zip bombs and path traversal.
- Every external format parser has a fuzzing target (libFuzzer) and runs in CI with ASan/UBSan.
- Reverse engineering does not justify an unsafe parser.

---

## 19. Logging and errors

### Logging
- Structured logging through a facade in `libs/base` (no Qt), with a sink that can forward to
  `QLoggingCategory` in the app.
- Categories: `media`, `decode`, `encode`, `gpu`, `compositor`, `timeline`, `project`,
  `importer`, `audio`, `cache`, `playback`, `ui`.
- **No stray `printf`/`std::cout`/`qDebug()`** as observability.
- Logging on the hot path: no allocation, no expensive per-frame formatting; use levels and
  rate limiting.
- Logging from the audio RT thread is forbidden (use counters/lock-free queues).

### Errors
- APIs between subsystems return `std::expected<T, oma::Error>` (alias `Result<T>`).
- `oma::Error` has: a typed code (enum class), category, message, context (file/stream/object).
- **Exceptions do not cross library boundaries.** If a dependency throws, catch at the
  boundary and convert.
- Silent failure is forbidden. Degradation (e.g. fallback to SW decode) is logged with the reason.

---

## 20. ADRs

Record in `Docs/adr/NNNN-title.md` (context, decision, alternatives, consequences).
Mandatory for decisions about: graphics backend, threading model, time representation,
project format, GPU interop, cache, import system, large new dependencies.

The current list and the pending ADRs are in `Docs/adr/README.md`.

---

## 21. Rules for AI agents

### Before changing the architecture
1. Understand the affected module and read its public API.
2. Check existing interfaces before creating new ones.
3. Identify the impact on **lifetime** and **threading**.
4. Assess the effect on **CPU↔GPU transfers**.
5. Check existing tests and what needs new tests.
6. Explain structural changes in the reply/PR. If it is an ADR decision, write the ADR.

### Always
- Follow the dependency rules in §5.2. A violation is a bug, even if it compiles.
- Keep changes focused on the request. Do not rewrite large parts for aesthetic preference.
- Do not replace chosen technologies without a measurable technical reason.
- When you find a real architectural problem: document it (issue/ADR) and propose a fix;
  do not silently work around it.
- Do not claim something is "fast", "real-time" or "zero-copy" without a benchmark or
  evidence (e.g. verified absence of readback).
- Do not invent performance numbers as targets.
- Update this file when a rule or command changes.

### Never
- Call FFmpeg outside `libs/media`; include Vulkan/CUDA in timeline/project/IR/importers.
- Put heavy logic in JS/QML.
- Use `double` as a reference timestamp.
- Create ad hoc threads; block the UI thread or the audio callback.
- Drop external project data silently.
- Implement DRM/license/encryption/authentication bypasses.
- Add a plugin system, AI or cloud without an explicit decision.
- Commit copyrighted media or large fixtures.

### Ask before
- Adding a new dependency.
- Changing the project format, time representation or threading model.
- Starting support for a proprietary binary format.
- Diverging from the development order in §24.

---

## 22. C++ code

Prefer: explicit ownership, RAII, strong types (IDs and units with their own types, not raw
`int`/`std::string`), const correctness, small APIs, `enum class`, explicit error handling,
clear names.

Avoid: globals, indiscriminate singletons, unnecessary macros, unsafe casts
(`reinterpret_cast`/C casts without justification), `shared_ptr` as the default choice, deep
inheritance, exceptions crossing subsystems.

Conventions (ADR-0001; `.clang-format` and `.clang-tidy` at the root are the reference):
- Types and enum values `PascalCase`; functions and variables `snake_case`; private members
  with a `_` suffix; constants `kPascalCase`.
- Do not use `describe`, `test`, `it`, `expect`, `bench`, `beforeEach`, `afterEach`,
  `beforeAll`, `afterAll` as API names: they are Cest macros in tests.
- Headers with `#pragma once`.
- Identifiers, code comments, log messages and all docs in English.
- Comments explain **why** (invariants, synchronization, the reason for a copy), not the obvious.
- Warnings are errors (`WERROR=1` by default; full set in ADR-0001).
- Vulkan objects with their own RAII or a thin wrapper; no manual `vkDestroy*` scattered around.

---

## 23. Performance

Performance is a functional requirement.

In critical code (decode, compositor, playback, audio):
- **Measure before optimizing.** Profile with perf, Tracy or RenderDoc/Nsight/RGP as appropriate.
- Zero allocations per frame on the hot path in steady state; use pools and reused buffers.
- Avoid large copies; prefer views (`std::span`) and handles.
- Control GPU resource lifetime explicitly.
- Avoid unnecessary GPU↔CPU synchronization.
- Instrument important zones from the start (a profiling macro/facade that can be disabled
  in release builds).

### Benchmarks (`tools/bench`)
Initial scenarios: N simultaneous 1080p60 streams, real-time compositing, A/V sync, timeline
interaction latency, 4K on reasonable hardware.

Record as JSON: FPS, frame time (mean, p95, p99, max), dropped frames, CPU, GPU, VRAM, RAM,
upload/download bandwidth when relevant, **plus hardware, driver, FFmpeg version, codec and
the decode path used**. Results without that context are useless for comparison.

---

## 24. Development order

**Detailed plan with milestones, done criteria and releases: `Docs/implementation-plan.md`.**
Update the plan when a milestone is finished. The list below summarizes the order.

Not rigid, but prioritize:

1. Build system (Make, `base`, tests, sanitizers, clang-format/tidy)
2. Media probing
3. Decode (SW first for correctness, then HW)
4. GPU frame abstraction
5. Vulkan compositor
6. Preview
7. Playback clock
8. Audio synchronization
9. Timeline model
10. Timeline UI
11. Project persistence
12. Import/export
13. Effects
14. Advanced interoperability

**Do not start with the final visual look.**

### First technical experiment (before a large UI)
A prototype that validates the pipeline:

```
video A + video B + image -> GPU compositor -> preview
```

Criteria: open several videos; decode them; keep frames on the GPU when possible (log where
not); composite layers through Vulkan with transform, scale, crop and opacity; present a
preview; synchronized playback. Instrument from the start: frame time, dropped frames,
uploads/readbacks. It may be an executable in `tools/` or `apps/` with a minimal window;
reusable code goes into the libs instead of staying stuck in the prototype.

---

## 25. Tests

Mandatory for structural components. Priority: timeline operations · time conversion ·
serialization · project migration · importers · ProjectIR · render graph · frame lifetime ·
parser robustness.

- Every bug fixed in a structural component gets a regression test.
- Render graph tests do not need a GPU. GPU tests are marked and can be skipped on machines
  without Vulkan.
- Image comparison with a tolerance against `CpuBackend` reference output.
- Small, reproducible fixtures in `tests/fixtures/`, with a **generator script** (e.g.
  `ffmpeg -f lavfi -i testsrc2=...`) versioned next to them. No copyrighted media.
- For RE: pairs of projects with minimal known changes (one field per pair), when legally
  possible, documenting what changed between them.
- TSan for concurrent code; ASan/UBSan for everything; fuzzing for parsers.

### Writing tests with Cest
- One suite per file `tests/<lib>/test_<topic>.cpp`, exposed as `void run_<topic>_tests()`
  and called from the lib's `main.cpp`. A new lib suite: `tests/<lib>/module.mk` with
  `oma_test` and an `include` in the `Makefile`.
- Include `oma_test.hpp` **last** (after project and standard headers).
- **No top-level commas** inside `describe`/`it`: the preprocessor splits macro arguments.
  Avoid `std::pair<A, B>`, `[&a, &b]` captures and `{1, 2}` directly in the block; use
  `auto`, `[&]`, helper functions or parentheses.
- `expect()` only on the test thread; results from other threads go through atomics.
- Enums and types without a Cest overload: compare `static_cast<int>(...)` or a boolean.

---

## 26. Implementation philosophy

Between two approaches, prefer the one that: preserves architectural separation · reduces
memory copies · makes testing easier · keeps APIs explicit · enables profiling · avoids
hidden dependencies · allows replacing backends.

But: do not create abstractions without a real need, do not generalize prematurely, do not
build plugins before there is a stable architecture. An abstraction is justified by a second
concrete use, by a subsystem boundary defined in this document, or by testability.

### The final question for every change

> Does this implementation keep OmaMovie simple for the user without making its architecture simplistic?
