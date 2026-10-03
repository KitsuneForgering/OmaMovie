# Hardware strategy after skeptical review

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Supported direction

Use the existing Vulkan compositor with explicit resource ownership, bounded work and a
logged software-decode fallback. Preserve the ADR-0004 application-owned device design,
while keeping Qt integration provisional until Q1/S4. No vendor-wide real-time or zero-copy
promise follows from one Intel report.

## 2. Roles and alternatives

| Component | Current direction | Evidence boundary / alternative |
|---|---|---|
| Vulkan | Composition, effects, optional video/compute codecs | Query device/profile/features and verify output |
| VA-API | First decode candidate on the reference Intel setup | Import may fail; software + upload remains baseline |
| Vulkan Video | Candidate when normally exposed | Debug-flag tests are exploratory, not supported defaults |
| NVDEC/NVENC | NVIDIA candidates | S7 pending; compare native Vulkan and total interop cost |
| CUDA effects | Optional specialization | Need an actual effect and same-output measurement |
| OpenCL | Excluded by project scope | Generic interop exists; no current use case |
| libplacebo | Color/scaling candidate | S6 and ADR-0006 pending |
| VMA | Allocation candidate | Existing wrappers first; adopt on demonstrated need |

[API/source evidence](sources.md) establishes contracts, not total pipeline performance.

## 3. Runtime policy

Distinguish five states: available API/library, advertised capability, successful open,
actual negotiated hardware frame, and validated path for this workload. Only the last
supports a compatibility/performance claim. Log skipped/failed paths and reasons.

ADR-0004's VA-API → Vulkan → software order is provisional for the measured Intel setup.
Do not infer NVDEC availability from `libnvcuvid` presence. Match device identity for imports;
probe support at codec/profile/chroma/depth/size granularity. Driver-specific policy changes
need evidence, not a universal vendor ranking. Software fallback is correct behavior and
still needs a workload budget; it is not guaranteed real time.

## 4. Device and synchronization choices

A shared device reduces explicit inter-device imports. A device created by FFmpeg could also
be handed to a consumer; it does not inherently force a second device or copy. The reason
for OmaMovie ownership is control over creation/features/lifetime (current ADR), not that
other ownership is impossible.

Queues need host synchronization; images need GPU dependencies, layouts and lifetime.
Qt's ordinary imported-device path has an identified flagged-queue mismatch (Q1).
An application mutex protects Qt only if Qt submissions actually participate in that
protocol. Decide among render-thread submission, controlled rendering, verified native
queue injection, or separate-device external-memory import in S4/ADR-0005.

## 5. Experiments and gates

| Spike | Review status | Required additional evidence |
|---|---|---|
| S1 | Prior Intel report, not rerun | Retain capability vs executed-codec distinction; raw output/provenance |
| S2 | Prior single-submitter report, not rerun | Validation layers, chroma/multiple frames, lifetime/cancellation |
| S3 | Pending | Repeated equivalent workloads; decode + import + consumer, encode quality |
| S4 | Pending; Q1 identified | Queue retrieval, submission/lifetime protocol, display conversion |
| S5 | Library tests contain timings | End-to-end playback, percentiles/drops, independent color oracle |
| S6 | Pending | Installed libplacebo contracts, image-quality and copy comparisons |
| S7 | Pending; NVIDIA unavailable | Decode+interop+composition and export on actual NVIDIA |
| S8 | Pending; AMD unavailable | Same criteria on actual AMD |

Protocols and stop criteria live in [the audit](skeptical-review.md#validation-protocols).
No pending experiment is relabeled as completed by this documentary review.

## 6. Capacity model

Use measured resource budgets, not only throughput. Tightly packed 4K RGBA16F is 63.28 MiB
per frame; three streams with eight such cached frames consume about 1.48 GiB before decoder
pools, intermediate targets and padding. This is a calculated lower-bound illustration,
not measured VRAM usage. [Inputs and calculations](skeptical-review.md#quantitative-checks).
On integrated GPUs, GPU-local allocation does not imply physically separate VRAM.

## 7. Application decision

Continue bounded Intel correctness work. Resolve Q1 and SDR/display handling before wiring
the final UI. Support claims must name tested hardware/drivers/media; untested AMD/NVIDIA
remain planned. Choosing a proxy codec also needs storage, seek, quality and compatibility
measurements; GPU compute decode alone does not justify FFv1/ProRes as the default.
