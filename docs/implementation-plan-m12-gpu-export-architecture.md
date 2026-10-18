### M12 — GPU-native export and application ownership (post-v0.1)

**Status: planned.** This is a separate improvement phase, not a prerequisite for the
scoped Intel/software v0.1 decision. Correctness or reliability failures discovered here
may require a v0.1 fix through M7/M10; architectural preference alone does not. M11 keeps
its existing targeted UI/ownership and responsiveness checks. Do not reopen closed work.

**Problem and code evidence (2026-10-15).** `export_timeline_on` constructs
`FrameSource(nullptr)`, so export decodes in software. Its Vulkan branch composites those
frames, then calls `read_nv12()` for the VA-API writer or `read_display()` for software
encoding (`apps/omamovie/src/exporter.cpp`). `EncodedFrame` owns CPU pixels in a vector;
the current NV12 compute pass writes a host-visible readback buffer, not an exportable
NV12 image (`libs/compositor/src/nv12_pass.cpp`).
`EncodeStage` passes NV12 bytes to `VideoWriter::write_nv12`, which copies rows into an
AVFrame and calls `av_hwframe_transfer_data` to upload to a VA-API surface
(`libs/media/src/video_writer.cpp`). The original `--export-audit` grouped composition and
readback in one timing; the first transfer counters below expose several boundaries but
still combine NV12 conversion, GPU wait and host copy. They cannot prove a zero-copy claim. Preview's
GPU decode and image import are separate implemented paths, not export validation.

`Session` owns export jobs/progress and other project, edit, playback, viewer, media and
recovery state (`session.hpp`); `TimelineView.qml` and `Main.qml` still use ancestor context
such as `session`, `root` and `actions`. Existing `ClipDrag` and incremental/compact
storyline paths are useful boundaries and performance constraints, not reasons to build a
new UI framework. [M11 inventory](analysis/m11-inventory-2026-10-14.md) found that a
transport owner would need too many shared inputs, so it is not an automatic extraction.

#### Sequence: one vertical change at a time

1. **Transfer baseline, first task.** Instrument the present export at decode output,
   compositor input/upload, color conversion, readback, writer CPU copy and VA-API upload.
   Count frames and bytes per boundary and time GPU waits/readbacks separately from render
   and encode. Record path and downgrade reason. Run the same pinned 1080p60 Intel fixture
   and at least one 4K fixture, with driver, codec, profile, resolution, rate, effects,
   hardware/software choice and repeat count in `docs/Research/evidence/`. Preserve the
   existing independent output decode. *Partial (2026-10-15):* explicit software-input,
   NV12 readback, writer row-copy and VA-API upload counts/bytes/timings are instrumented;
   a [single-run 1080p60/4K baseline](Research/evidence/2026-10-15-export-transfer-baseline.md)
   and independent decodability/frame-count checks exist. The readback call still combines
   conversion, wait and host copy; software input bytes are source payload, not bus traffic.
   Separate those stages, process CPU/frame, memory/GPU use and repeat runs before using
   this as a before/after performance comparison. Avoid claiming driver-internal copy freedom.
2. Define a GPU-native encoder input contract at the media/encoder boundary: image or
   surface handle, format (NV12 now; P010-capable design), color metadata, rational PTS,
   ready/reuse synchronization and explicit lifetime. Keep the CPU-frame contract for
   fallback. Choose the concrete API only after tracing current `VideoFrame`, compositor
   output and `VideoWriter` ownership; do not add a universal backend interface.
3. Prove an Intel Vulkan output → exportable memory/DMA-BUF → VA-API surface import pilot.
   Check DRM modifiers, device identity, plane layout, ownership transfer, ready/reuse
   synchronization, pool capacity, cancellation and device loss. Record actual driver/API
   constraints in a new ADR before replacing the writer path. Do not infer AMD/NVIDIA support.
4. Keep working-space → NV12 conversion on GPU for the first path. Specify P010 planes,
   color tags and synchronization in the contract, but do not make P010 or HDR a v0.1 gate
   or claim HDR without a separately validated color pipeline. Compare frames against the
   existing independent decoder/oracle before removing the NV12 readback from this path.
5. Evaluate hardware decode during export using the existing `VideoDecoder`/`FrameSource`
   mechanism only after the encoder input works. Profile surface lifetime, device admission,
   queue contention and throughput; retain software decode if reuse serializes the pipeline.
6. Extract export ownership from `Session` only where its job lifecycle, pinned snapshot,
   progress, cancellation and publication/recovery state have a clear owner. Preserve
   `Session`'s QML properties/signals and the existing export/recovery tests. Do not make
   `ExportController` a wrapper around free functions or automatically absorb recovery.
7. Map and extract one QML subtree with explicit properties and intent signals, starting
   where `TimelineView`'s ancestor-ID coupling and repeated gestures actually change
   together. Keep engine edit state in `Session`/timeline. Preserve viewport filtering,
   compact storyline, incremental model notifications and delegate reuse. Measure the 5,000
   clip workload before and after each timeline extraction.
8. Reassess playback/viewer, project/recovery and media ownership separately. The current
   [transport note](analysis/m11-transport-owner-note.md) is a counterexample to extracting
   by class size. An `EditController` must own application behavior beyond `timeline::Editor`
   to justify existing.

#### Ownership map to confirm before extraction

Current observed owners: `Session` contains `editor_` (which owns the active timeline),
project path/state, media paths, `AudioPlayer`, `VideoScheduler`, software and hardware `FrameSource`s, thumbnail
and waveform caches, and worker/viewer/export job pools (`session.hpp`). An export request
copies/pins its timeline, paths and LUTs; export constructs its own Vulkan device and
compositor (`exporter.hpp`). Verify each lifetime and signal path in the implementation
inventory before moving it. The target is one owner per lifecycle with acyclic dependencies;
`Session` remains the QML facade. Controllers should consume explicit immutable state or
services, not call one another through an event bus or service locator.

```text
main: viewer GPU device → PreviewItem/Qt bridge (outlives Session)
Session: Editor → active Timeline; project path/media paths; AudioPlayer + VideoScheduler
         FrameSources; caches; worker/viewer/export JobPools; QML properties/signals
export job: pinned Timeline + paths + LUTs → export-owned GPU device/compositor
            → bounded EncodeStage → VideoWriter → staged movie/publication
```

Before an extraction, list each `Session` property/signal and its current producer,
thread and QML consumer. Keep the public facade initially and forward notifications
without cycles. Record lifetime order for jobs, FrameSources, pooled surfaces and devices.

#### Acceptance, evidence and fallback

- Report **zero-copy decode**, **compositor input**, **compositor output**, **encoder input**
  and **end-to-end** separately. End-to-end requires an instrumented Intel path with no
  mandatory CPU pixel readback/upload from decode through encode. A CPU-side audio mix or
  mux metadata does not by itself negate the video claim; hidden video readback does.
- Validate decoded output frames and frame count, color/frame fidelity, audio samples and
  A/V sync, cancellation, device loss, export publication/recovery and software fallback.
  Run 1080p60 and at least one 4K workload on available Intel hardware. Record throughput,
  CPU time per frame, GPU use where measurable, stage latency, peak memory and transfer
  counts/bytes against the baseline on the same fixtures and machine.
- Select a path explicitly: Intel zero-copy where negotiated and validated → current GPU
  staged path → CPU reference/software encoder. Log each downgrade with the failed boundary
  and reason. A device loss must preserve the current completed-output/recovery contract.
- For QML, rerun GUI smoke and manual keyboard/pointer/context-menu/source-viewer,
  roll/slip/slide, drag/trim, resize, text-scaling and accessibility checks. For Session,
  rerun affected app tests, GUI smoke, sanitizer subset and export/recovery tests. The
  5,000-clip latency and memory comparison must show no material regression or explain it.

**Risks:** DRM modifier incompatibility, VA surface import rejection, implicit driver
copies, synchronization deadlocks, extra pool memory, Qt/device contention, output color
drift, QML binding churn and signal/API regressions. Any failed pilot returns to the
validated staged path while evidence and design are revised.

**Out of scope:** AMD/NVIDIA support claims without hardware, universal GPU abstraction,
full HDR, complete QML or Session rewrites, React/Redux inside Qt, service locator, global
event bus, dependency framework, aesthetic file splitting and file-count/LOC gates.

**Done when:** the Intel decode → composite → encode video path passes the named correctness,
A/V, failure and 1080p60/4K checks without mandatory CPU pixel round-trip, with reproducible
before/after measurements and working fallback; at least the highest-coupled QML boundary
has explicit inputs/intents without long-timeline regression; independent lifecycle work
has left `Session` behind a stable QML facade with documented, acyclic ownership. An
unsuccessful interop pilot is recorded as a bounded finding, not marked done.

[← Implementation plan](implementation-plan.md)
