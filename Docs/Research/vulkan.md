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

[Continue in vulkan-boundaries.md](vulkan-boundaries.md)
