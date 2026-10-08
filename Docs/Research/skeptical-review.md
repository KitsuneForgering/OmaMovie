# Skeptical review of OmaMovie documentation

> VEGAS Pro 23 addendum (2026-10-07): [dedicated review](vegas-pro.md) and
> [source entries V1–V8](sources.md) add official-help evidence for source-range
> selection, ripple scope, sync links, takes and adjustment events. This is a
> documentary comparison, not a local VEGAS test or user result. M6 now has
> proposed source-selection and ripple-visibility pilots; M8 has conditional
> takes and adjustment-event pilots. Existing anchor, transition and proxy
> decisions stay subject to their own task/workload gates.

**Conclusion (2026-10-03):** retain the modular GPU-first direction and bounded Intel work.
Qt queue integration, color/display correctness and multi-vendor validation remain gates.
Product fit and the UI proposals need task evidence. The earlier research did not justify
universal zero-copy/real-time claims, an empty Linux market or competitor complaint rankings.

This review updates all `Docs/Research` topics and their dependent plan, UI, ADR and spike
notes. It changes documentation, not pipeline behavior. Existing maintainer preferences
and accepted ADR decisions are preserved; corrections to their evidence are explicitly dated.

## Question, decision and budget

Question: which research premises are reliable enough to guide the next implementation?
Target: the actual C++/FFmpeg/Vulkan/Qt/PipeWire editor on Omarchy, initially the reference
Intel machine. Baseline: current library code plus software decode/upload, conventional
track/inspector UI, and existing Kdenlive/Shotcut. Wrong GPU/sync/color assumptions risk
corrupt output and costly rework; wrong audience assumptions risk building unused features.

Initial hypotheses:

| Hypothesis | Evidence that would change it |
|---|---|
| H1: keeping frames GPU-accessible helps editing | Same-output end-to-end benchmark shows interop/scheduling cost outweighs transfer savings |
| H2: one owned device simplifies decode/compositor/Qt | Version-specific queue/lifetime contracts conflict or simpler handoff meets the same budget |
| H3: Vulkan Compute suffices for initial effects | A concrete effect/library requires another backend with measured total advantage |
| H4: contextual/magnetic UI helps simple edits | Task pilot shows hidden controls, surprise track moves or worse recovery than baseline |
| H5: local consumer-media workflows differentiate OmaMovie | Existing Linux tools perform the same tasks with comparable setup/performance on target hardware |

Budget: one repository-wide document inventory, primary-source inspection in technical and
product/platform passes, focused counterevidence checks, local source/environment inspection,
capacity/time arithmetic, and document consistency checks. Coverage favors consequential
premises over release trivia. No exhaustive literature review, GPU matrix rerun, competitor
installation, pricing/legal review or user study is claimed. Stop when the next decisions
are supported or explicitly gated; unavailable NVIDIA/AMD hardware and pending Qt/color
experiments prevent broader validation. [Source register](sources.md) records actual access.

## Foundations and premise review

| Decision-critical claim | Foundation and mechanism | Premise status and boundary | Discriminating check |
|---|---|---|---|
| Shared queue is safe | Vulkan host synchronization vs GPU dependencies, queue retrieval valid usage (K1/K2); Qt tagged code (Q3) | Contract verified; compatibility inference identifies Q1; runtime behavior pending | S4 with actual Qt build, validation layers, concurrent submission and teardown |
| Imported frame needs no host copy | External-memory allocation/format/layout/lifetime; FFmpeg frame contract (F2) | API/source contract documented; S2 historical report; driver-internal copies unmeasured | Source/trace transfer accounting, multiple-frame YUV comparison and reuse stress |
| Hardware decode is supported | Profile/extent/chroma/depth queries (K3) plus negotiated frame format | Capability listing is insufficient; one Intel observation cannot generalize | Same media per API/device, reject silent fallback |
| Float output gives correct color | Distinct input/working/output transforms (D1), explicit metadata | Current SDR code observed; complete HDR/display handling absent | Independent vectors/ramps, alpha/range/chroma, preview/export agreement |
| No OpenCL interop exists | Khronos external-memory/semaphore sample (K4) | Universal claim contradicted; installed runtime matrix unknown | Concrete kernel/runtime/import experiment only if scope is reopened |
| Rational timestamps avoid accumulated rounding | Exact arithmetic and explicit rescaling; sample/frame units | Preference justified for arbitrary timebases; does not imply all curve samples are exact | Boundary/overflow and TimeMap domain/interpolation tests |
| File save survives a crash | Linux durability contract (S1); file/directory sync are distinct | File sync alone does not prove rename durability | Failure injection and recovery on supported filesystems |
| Simple UI improves editing | Interaction semantics, task completion/errors/recovery, baseline comparison | Product hypothesis, no local users observed | M6 pilot; preserve precise track/time control |
| Linux codec friction exists | Versioned B2 matrix | Supported for named Resolve 20 rows; no generic NVIDIA/PipeWire exclusion | Current edition + same-file task comparison before external claims |

These references apply because they specify the affected interfaces/units/tasks. They do
not substitute vendor prestige for evidence. Multiple summaries of one announcement are
not independent corroboration. Documentary contradictions trigger revision of the exact
premise, rather than averaging incompatible versions or inventing controversy.

## Q1: Qt queue integration

Qt's tagged 6.11.2 Vulkan RHI retrieves its graphics queue with `vkGetDeviceQueue` (Q3).
Khronos requires zero creation flags for that call (K1); flagged queues require
`vkGetDeviceQueue2`. The current OmaMovie device enables internally synchronized queue flags
when supported. Thus the ordinary imported-device route has a **source-level mismatch**.
A patched installed Qt or verified native queue injection could change this conclusion.

No Qt runtime reproducer ran here. S4 must check the installed code path and choose a valid
retrieval and submission protocol. Alternatives: zero-flag queues with coordinated
render-thread submission; controlled rendering; supported queue injection; separate-device
external-memory import. A mutex used only by OmaMovie/FFmpeg does not synchronize Qt.
Internal queue synchronization also leaves image barriers, layouts, readiness and reuse
obligations intact. ADR-0005 must resolve these before claiming preview integration complete.

**Implementation follow-up (2026-10-03):** [S4](../spikes/S4-qt-shared-device.md) ran
the installed Qt 6.11.2 with zero-flag queues and verified matching device/queue handles,
borrowed RGBA16F image sampling, 60 sequential offscreen frames across two sizes,
diagnostic pixel checks and zero reported Vulkan validation errors. This narrows Q1 to
the concurrent submission and visible presentation protocol; the study above remains
the record of what was known before the runtime experiment.
The historical visible S4 run presented a static three-source scene; its
stable-frame contention and post-drain window rebuild passed validation.
`make run-gui` now opens user video through a temporary software-frame
preview. The current repeatable shared-image diagnostic is the offscreen S4
spike. M4 still needs to gate live workers across Qt swapchain lifecycle and
provide new-frame playback.

## Material revisions

| Earlier premise | Disposition and consequence |
|---|---|
| FFmpeg 9 announced August 2026 | Not corroborated by F1; local package/binary label is recorded separately; pin actual API provenance |
| Vulkan Video mature on all Intel/AMD codecs | Replace with profile/device/driver queries and negotiated-frame checks |
| NVENC universally ~5× faster than Vulkan encode | Withdraw ranking; compare equal-quality configurations in S7 |
| CUDA always forbids wait-before-signal | Withdraw blanket claim; specify exact semaphore type/version and liveness proof |
| OpenCL impossible without frame copies | Generic counterexample K4; keep exclusion as scope/maintenance choice |
| S2 is zero-copy by construction and proves correct content | Narrow to inspected application path and sampled luma; chroma, all frames, driver internals pending |
| FFmpeg ownership forces another device/copy | Incorrect generalization; ownership choice is about control and lifetime |
| iMovie uses FCP engine, proving our thesis | Transfer documented; internals not established; UI/pipeline thesis remains a hypothesis |
| Movie Maker 2012 proves semantic zoom | Historical behavior unverified; storyboard projection is our testable proposal |
| Resolve Linux means NVIDIA only / no PipeWire | Codec PDF is narrower; backend absence in a source is not incompatibility evidence |
| Stability is the main competitor complaint; market is empty | No representative sample; withdraw ranking and market-gap fact |
| Omarchy theme swap is atomic | Installed script removes then moves; adapter needs retries/last-good state |
| RGBA16F means HDR or accurate display | Storage precision is not color management; implement explicit transforms and oracle tests |
| Every TimeMap derives duration from source displacement | Freeze/reverse/ramps need domain and interpolation semantics; complement ADR-0002 before M5 |
| Leak is harmless ~200 KB per session | Prior report says per decoder; no retained raw reproduction here; broad suppressions can mask growth |

## Quantitative checks

Locally executed **arithmetic**, not a hardware benchmark. Assumptions: tightly packed even
4:2:0 frames, no padding/compression, decimal bandwidth, binary memory units. For `W × H`
pixels: NV12 = `1.5WH` bytes, P010 = `3WH`, RGBA16F = `8WH`. One full-frame transfer at rate
`f` costs `Bf` bytes/s; readback plus re-upload doubles that payload, not necessarily measured
time. Shared system memory on an iGPU still has traffic/synchronization costs.

[Calculation output](evidence/2026-10-03-calculations.txt): 4K RGBA16F = 63.28125 MiB;
one transfer at 60 fps = 3.981312 GB/s; 3 streams × 8 frames = 1518.75 MiB excluding other
allocations. 1080p60's presentation interval is 16.6667 ms. A 3 ms isolated compositor sample
does not establish decode/audio/UI/presentation deadlines or tail latency.

Reproduce the capacity arithmetic:

```python
for w, h in [(1920, 1080), (3840, 2160)]:
    for name, bpp in [('NV12', 1.5), ('P010', 3), ('RGBA16F', 8)]:
        b = int(w * h * bpp)
        print(w, h, name, b, b / 2**20, b * 60 / 1e9)
print(1000 / 60, 3840 * 2160 * 8 * 3 * 8 / 2**20)
```

A constant 2× speed over 10 s consumes 20 s of source; a linear speed ramp 1×→2× over
10 s consumes 15 s (`integral s(t) dt`). A frozen 10 s domain consumes zero source duration.
These dimensional/boundary cases refute a duration rule based only on source displacement.
The arithmetic is locally checked; the TimeMap implementation remains proposed.

## Validation protocols

All protocols below are **proposed, not executed in this review**. Set scene/settings and
acceptance values before collecting results; record changes to exploratory criteria.

| Gate | Inputs, comparator and measurements | Acceptance / rejection |
|---|---|---|
| S2 follow-up | Synthetic CFR/VFR, 8/10-bit, chroma/color patterns; SW reference plus independent vectors; many frames, seeks, failed submits, shutdown | No validation-layer errors/corruption/stale reuse; bounded lifetime; count explicit copies and state driver-copy visibility limits |
| S3/S5/S7/S8 performance | Fixed representative scene, identical decode/output and encode-quality settings; warm-up + at least 5 repetitions, end-to-end timestamps, p50/p95/p99/max, drops, CPU/GPU/RAM/memory, power if instrument available | For claimed 1080p60 scene, proposed p99 ≤ 16.67 ms and no sustained drops over 10 minutes; reject hidden fallback/output mismatch/unbounded growth; means alone do not pass |
| S4/Q1 | Actual Qt/Wayland session and build, queue flags/retrieval, 2 videos + image, resize/minimize, decode+render concurrency, cancellation/teardown | Valid queue retrieval and dependency protocol; no validation errors/deadlock/readback in preview; explicit display transform; unsupported flag path must fail or select verified alternative |
| S6/color | Current shaders vs installed libplacebo and independently specified SDR vectors; full/limited ranges, primaries/transfer, alpha, chroma siting, missing metadata, PQ/HLG | Specify supported SDR errors/tolerances before measurement; correct fallback/notice for unsupported HDR; CPU/GPU agreement alone is insufficient |
| M4 audio | Synthetic audiovisual sync impulses plus real recordings, 10-minute runs, 44.1/48 kHz, device changes, seeks, underruns, no-audio case | Proposed sync pilot bound ≤ one output-video-frame interval after startup/seek settles, with no growing drift; define presentation/latency measurement; TSan does not validate RT deadlines |
| M6 UI | Same task sequence in drawer and side-inspector/ordinary-track baselines, wide/half/narrow, different fonts, mouse/keyboard | All required actions reachable, recoverable undo, no surprise linked-track corruption; record errors/time and revise on regressions; no population preference from a tiny pilot |
| M7 persistence | Versioned fixtures, forced failures at write/sync/rename/directory-sync, disk full, old/new versions | Recover old or complete new file, no corrupted acknowledged save; errors surfaced; durability statement names tested filesystem |
| M9 interchange | Pinned OTIO library/schema, actual app exports with known edits, fractional/unsupported times and effects | Preserve declared subset and explicit conversion error/report; no silent data loss or unproved round-trip claim |

## Verification status and stopping decision

- **Executed locally now:** read-only package/API/script/code inspection, hashes/environment
  capture, capacity and speed-boundary arithmetic; existing debug test suites and final document checks recorded below.
- **Prior locally reported experiments:** S1/S2 and M0/M3 test/timing results. They retain
  historical labels, with limits; they were not independently rerun in this review.
- **Published third-party results:** no independent comparative benchmark is adopted as
  decisive. Source descriptions, roadmaps and vendor examples are documentary evidence.
- **Documentary analysis:** primary interface/source inspection and versioned codec rows.
- **Not executed:** GPU concurrency/Qt pilot, multi-vendor benchmarks, independent color
  validation, crash-injection, importer/user studies. Required hardware/data remains missing.

Apply current design only within its demonstrated scope. H2 has a material compatibility
challenge (Q1); H4/H5 remain product hypotheses. Research coverage is sufficient to correct
the documents and identify the next gates, not to certify release readiness. Resume the plan
with those gates and the existing code state, rather than restarting completed S1/S2.

### Final local checks

- 84 local Markdown file links/heading anchors checked; no dangling targets at that check.
- `git diff --check` passed after the documentation edits.
- `make test` first failed at Vulkan initialization inside the sandbox. The authorized
  outside-sandbox retry exited 0: existing `base`, `gpu`, `media`, `compositor` debug suites
  passed. [Retained output](evidence/2026-10-03-make-test.txt). Cest's printed counts are
  retained as emitted, not reinterpreted as independent test-case counts.
- The retry's existing isolated compositor timing printed 3.93 ms (one warm-up, mean of
  20 submissions); it is not S3/S5 end-to-end validation. Conditional hardware tests may
  accept an unsupported path; this pass does not validate all hardware/codec branches.
- Sanitizers, Qt preview, independent color tests and the proposed pilots were not run.
  User work in `Makefile`/`libs/audio` was preserved; no pipeline code changed.
