# ADR-0004 — GPU frame abstraction, synchronization and decode policy

- **Status:** Accepted (2026-10-03)
- **Milestone:** M2 (`libs/gpu`, `libs/media`); revisited when NVIDIA/AMD hardware is measured

## Evidence review (2026-10-03)

The accepted core decision remains in force; the evidence boundary is narrower than some
original context/alternative wording. S1/S2 are prior Intel reports, not rerun by this audit;
no-host-transfer observations and small luma samples do not certify driver-internal copies,
chroma/full-frame accuracy, multi-submitter correctness or other GPUs.

**Qt remains pending.** [Q1](../Research/skeptical-review.md#q1-qt-queue-integration) identifies
a source-level flagged-queue retrieval mismatch in tagged Qt 6.11.2. Resolve it in S4 and
ADR-0005; internally synchronized queues address host access, not image readiness/layouts.
A fallback mutex must include actual Qt submissions to protect that shared queue.

The alternative statement that FFmpeg-owned devices inherently require another device/copy
is too strong: a consumer can potentially borrow that device. OmaMovie ownership is retained
for creation/control/lifetime under this decision, not because the alternative is impossible.
Decode order is scoped to the measured Intel setup until equivalent driver/device evidence.

The reported ~200 KB leak is per decoder, not a guaranteed session bound. The checked-in
LSan file includes broad functions and an OmaMovie wrapper, so suppression does not prove
absence of project leaks. Require an unsuppressed minimal reproduction and repeated-decoder
lifetime/growth check before relying on it for release readiness. Do not silently change
this accepted ADR or pipeline behavior as part of a documentation audit.

## Context

The pipeline is GPU-first (CLAUDE.md §7): decoded frames should reach the compositor as Vulkan
images on OmaMovie's device, without a CPU copy. Spikes S1 and S2 (`docs/spikes/`) measured the
reference machine (Intel Iris Xe, Mesa 26.2, FFmpeg 9.0.1):

- VA-API decodes H.264, HEVC 8/10-bit, AV1 and VP9 by default; Vulkan Video is only exposed
  behind `ANV_DEBUG=video-decode`, and AV1 fails there even then.
- FFmpeg accepts a `VkDevice` created by OmaMovie and imports VA-API surfaces into it through
  DMA-BUF without a copy (`av_hwframe_map`, without `AV_HWFRAME_MAP_DIRECT`).
- FFmpeg silently continues in software when the hardware refuses a stream (profile, size),
  so the negotiated format has to be checked.
- `VK_KHR_internally_synchronized_queues` lets FFmpeg, the compositor and Qt submit to the same
  queue without an application lock; FFmpeg 9 deprecates its queue-lock hooks in its favor.

## Decision and outcome

- [Policy, alternatives and consequences](0004-policy.md)
- [2026-10-06 amendment](0004-amendment.md)
