# OpenCL: scope decision and corrected rationale

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

Keep OpenCL excluded under the existing 2026-10-02 maintainer decision. Vulkan Compute
already addresses compositor-resident effects; an additional backend has no demonstrated
project use case. This is a maintenance/scope preference, not a finding that OpenCL cannot
share memory with Vulkan.

## 2. Counterevidence

[Khronos's interop sample](https://docs.vulkan.org/samples/latest/samples/extensions/open_cl_interop/README.html)
describes sharing external memory and semaphores and matching devices by UUID. It refutes
an API-level impossibility claim. It does not establish support on every runtime or image
format, nor performance on OmaMovie workloads.

[Mesa Rusticl documentation](https://docs.mesa3d.org/rusticl.html) describes Gallium-based
OpenCL and distro-configurable default device enabling. This review did not establish an
external-memory extension matrix for installed Rusticl/NEO/ROCm. The prior absolute claim
that Rusticl has no zero-copy Vulkan interop was unsupported for the specified 2026 build.
Unavailability of evidence is not evidence of absence. The old OpenCL 3.1 runtime table is
withdrawn pending direct runtime queries/versioned release evidence.

## 3. Comparison under equivalent criteria

| Criterion | Vulkan Compute | OpenCL alternative |
|---|---|---|
| Current compositor storage | Same-device access with barriers | Requires compatible external-memory/sync contracts |
| Additional project work | Existing GPU integration | Runtime discovery, import and kernel/backend maintenance |
| Existing required effect | Can implement initial effects | No identified effect requiring it |
| Portability of numeric features | Query shader features | Query device/version/extensions; do not assume precision |
| Performance | Pending effect benchmark | Pending same-effect, same-GPU benchmark including transfers |

Installation defaults are version-sensitive convenience evidence. They do not determine
whether a packaged application can declare a runtime dependency. No desktop runtime changes
were made during this review.

## 4. What would reopen the decision

An existing OpenCL library/kernel must solve a concrete effect better than the baseline,
with representative output correctness, import/sync/lifetime and total latency measured.
Record that evidence and a new ADR before adding the backend. Blender's Cycles choices were
for a different workload and do not prove the choice for a video editor.

## 5. Validation status

Documentary analysis only. No OpenCL runtime/kernel/interop experiment ran here. Generic
API support is documented; the target-device extension matrix remains pending. Existing
Vulkan + CPU reference work continues without implementing three backends at once.
