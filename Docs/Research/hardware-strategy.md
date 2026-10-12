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

[Continue in hardware-validation.md](hardware-validation.md)
