# Vulkan: supported contracts and pending integration

> Reviewed on 2026-10-03 with skeptical-research. This is non-normative research.
> Source metadata and access limits: [source register](sources.md). Decisions and open
> validation gates: [audit](skeptical-review.md). Product descriptions are not user studies.

## 1. Conclusion

Retain Vulkan for compositing and effects. Treat one application-owned device as the
current ADR-0004 design. S4 resolved Qt's queue-retrieval issue for a static
same-device preview; M4 live handoff remains open. API availability is distinct from
codec support, successful initialization, correctness and playback deadlines.

## 2. Bindings and allocation

[Khronos Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp) documents typed C++ bindings
and RAII. The installed build uses exception-disabled bindings and `std::expected`; macros
must be checked against the installed headers. RAII owns resources only when ownership is
transferred: wrapping borrowed FFmpeg/Qt objects in owning handles would double-destroy them.

A PCH is an existing build choice, not evidence that C++ modules are needed. VMA remains a
candidate; the repository already has allocation wrappers in `libs/gpu/src/resources.cpp`.
Compare their fragmentation/lifetime needs before adding a dependency. Never route imported
memory through an allocator without an explicit external-memory ownership contract.

## 3. Video capabilities

[FFmpeg announcements](https://ffmpeg.org/index.html) document Vulkan H.264/HEVC encode in
7.1, FFv1 compute and ProRes RAW decode in 8.0, and ProRes compute encode/decode in 8.1.
This is implementation availability, not a universal hardware capability table.
The claimed August 2026 FFmpeg 9 release was not corroborated by that page; local `n9.0.1`
is independently recorded in [environment evidence](evidence/2026-10-03-local.txt).

[Vulkan video queries](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceVideoCapabilitiesKHR.html)
are profile-specific. Check codec operation, profile, chroma, bit depth, coded extent,
queue support, image format/usage and actual negotiated output. A listed decoder or shared
extension name is insufficient. S1's Intel debug-flag result does not describe all ANV/RADV GPUs.

## 4. VA-API mapping

S2 reports VA-API → DMA-BUF → Vulkan on an OmaMovie-owned device, with separate plane images.
Reuse FFmpeg mapping where the installed implementation supports it; preserve source frame
references, DRM modifier/stride/offset and GPU identity. Multi-planar and separate-plane
layouts both require explicit handling. Extension presence does not guarantee an import.

Without a direct-map guarantee or driver trace, call the result **no explicit host transfer
in the inspected path**, not proof of every driver-internal copy being absent. S2's first-frame
luma check is a verification readback; it is not part of steady-state playback.

## 5. Frame contract

[FFmpeg's Vulkan header](https://ffmpeg.org/doxygen/trunk/hwcontext__vulkan_8h_source.html),
also inspected in installed libavutil 61.1.101, specifies per-image timeline semaphore values,
layout/access state and frame locking. Consumers must respect these fields, source lifetime
and queue-family ownership, including failed submission and cancellation.
Queue host synchronization and image dependency synchronization solve different problems.
An internally synchronized queue does not supply the latter automatically.

## 6. Qt preview: gate Q1

[QQuickGraphicsDevice](https://doc.qt.io/qt-6/qquickgraphicsdevice.html) can reference an
external device. [QSGVulkanTexture](https://doc.qt.io/qt-6/qnativeinterface-qsgvulkantexture.html)
wraps a borrowed 2D RGBA image on the rendering thread; its API does not take a readiness
semaphore. Presentation needs a separate handoff/lifetime protocol and a display transform.
RGBA16F storage alone does not mean a correct SDR or HDR preview.

The Qt 6.11.2 source/API mismatch with flagged queues is described in [Q1](skeptical-review.md#q1-qt-queue-integration).
Do not claim S4 done because the factories exist. Compare render-thread submission,
controlled rendering, and separate-device import under identical output/deadline checks.

The subsequent [S4 experiments](../spikes/S4-qt-shared-device.md) verified
the zero-flag queue path, offscreen RGBA16F sampling, visible presentation and
stable-frame queue contention. Qt's swapchain lifecycle calls `vkDeviceWaitIdle`
outside frame signals, so M4 must stop live workers before those events.

## 7. libplacebo and color

[libplacebo's Vulkan header](https://raw.githubusercontent.com/haasn/libplacebo/master/src/include/libplacebo/vulkan.h)
contains external-device import, wrapped-image and synchronization contracts. Inspect the
installed version before integration: `master` is not an ABI pin. Sharing constraints still
apply. S6 compares it with current shaders using independent color vectors, memory/copy
accounting and representative image quality; advertised quality is not our measurement.

## 8. Application boundary

S1/S2 are prior reported experiments; this review did not rerun them. Retain software decode
and one explicit upload when hardware/import fails. NVIDIA encode preference needs S7;
there is no verified universal NVENC/Vulkan speed ratio. See the audit protocols for repeated
end-to-end measurements and the shader color limitations already present in the source.
