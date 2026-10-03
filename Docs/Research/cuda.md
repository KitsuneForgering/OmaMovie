# CUDA and NVIDIA: runtime, interop and comparison

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

NVDEC/NVENC are candidates through the already packaged FFmpeg. CUDA effects remain optional
and require measured advantage. No NVIDIA GPU result exists in this review; S7 gates support.
The former approximately 5× encode claim is withdrawn as a selection rule: a forum report
cannot rank different settings, quality, hardware and driver versions for this project.

## 2. Runtime versus build

[NVIDIA's FFmpeg guide](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/ffmpeg-with-nvidia-gpu/index.html)
distinguishes runtime use from compiling its suggested toolkit/NPP-enabled configuration.
Its binary can run without the toolkit. For OmaMovie, linking the system FFmpeg is different
from rebuilding FFmpeg or writing CUDA kernels. Check the actual FFmpeg configuration and
SDK/driver compatibility; installed libraries alone do not establish supported devices/codecs.
[FFmpeg nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) is the interface source,
not proof that every FFmpeg configuration builds without any toolkit components.

## 3. CUDA ↔ Vulkan

[NVIDIA's external-resource API](https://docs.nvidia.com/cuda/cuda-runtime-api/cuda_runtime_api/group__CUDART__EXTRES__INTEROP.html)
documents handle-specific memory mappings and semaphore semantics. Binary and timeline
semaphores have different behavior; the old blanket prohibition on CUDA wait-before-signal
was not established by the inspected reference. Specify the exact handle type/API/version
and prove scheduler liveness rather than applying a forum observation universally.

Possible path: export Vulkan storage, import in CUDA, copy NVDEC output device-to-device,
then synchronize Vulkan consumption. That GPU copy is not allocation-level zero-copy.
Matching the physical GPU, pitch/planes, allocation flags, handle ownership and teardown
are prerequisites. FFmpeg CUDA↔Vulkan mapping support must be checked in the installed
source/API and exercised; it is not assumed by this document.

## 4. Kernel build

PTX loaded by the driver and a Clang build are possible future approaches, not a mandated
build architecture. Compiler/toolkit/libdevice compatibility, PTX target, initialization
cost and distribution terms need a concrete kernel/configuration review before adoption.
Do not make toolkit size a permanent architectural premise. Do not copy a vendor guide's
`--enable-nonfree` configuration into releases without the packaging license check.

## 5. S7: proposed discriminating check

Compare NVDEC + Vulkan interop, native Vulkan decode and software + upload on one NVIDIA
GPU. Use identical media, verify frame format/content, count host and device copies, and
report seek latency, end-to-end frame times, CPU/GPU/memory and initialization costs.
For encode compare codec/profile, quality or bitrate, rate control, preset, GOP and output
correctness as well as throughput. A faster encode at lower quality is not an equivalent win.
Reject a path on corruption, synchronization errors, unbounded memory or hidden fallback.

## 6. Hybrid systems and scope

One selected GPU per pipeline is a preference that limits cross-device complexity. Verify
CUDA UUID against Vulkan identity and the decode render node. Presentation can still involve
the desktop compositor's other GPU; one application device does not remove that cost.
Power-safe enumeration is unvalidated: capability queries may load drivers or wake devices.
Record power behavior before claiming otherwise. No NVIDIA/hybrid benchmark was run.
