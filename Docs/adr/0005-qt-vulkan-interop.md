# ADR-0005 — Qt Quick and the application-owned Vulkan device

**Status: Accepted (2026-10-03).** The S4 diagnostic validated device import;
the editor's viewer now presents compositor images this way (see
*Implementation in the viewer*). Admission for producers on other threads
(hardware decode, export) remains implementation work.

## Context

ADR-0004 chose one application-owned `VkDevice` and internally synchronized
queues when available. The installed Qt 6.11.2 Vulkan RHI, when given an
external device, calls `vkGetDeviceQueue`. Vulkan requires zero queue creation
flags for that retrieval call; flagged queues need `vkGetDeviceQueue2`. Qt's
native handles expose the graphics queue for inspection, not injection. See
[S4](../spikes/S4-qt-shared-device.md) and the [Q1 evidence](../Research/skeptical-review.md#q1-qt-queue-integration).
The precise interfaces are in [Qt's tagged RHI source](https://github.com/qt/qtbase/blob/v6.11.2/src/gui/rhi/qrhivulkan.cpp),
[Qt's RHI native-handle header](https://github.com/qt/qtbase/blob/v6.11.2/src/gui/rhi/qrhi_platform.h)
and the [Vulkan queue retrieval rule](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceQueue.html).

## Decision

Keep OmaMovie as owner of one device. For a Qt Quick bridge using the installed
Qt API, request zero-flag queues through `gpu::DeviceOptions`. The option leaves
existing headless users of ADR-0004 unchanged. With zero-flag queues, all
concurrent queue access requires external host synchronization. The Qt bridge
locks OmaMovie's graphics queue from `beforeFrameBegin` through `afterFrameEnd`
using direct render-thread signal connections. The installed Qt threaded render
loop places `endFrame` (including presentation) inside that interval. Producers
must stop and drain before Qt creates/resizes/destroys its swapchain, because Qt
calls `vkDeviceWaitIdle` outside the signals. S4 proves this admission sequence
with a separate job submitting 120 command buffers while the window redraws.
M4 must implement the same boundary for actual decoded frames and arbitrary
window lifecycle events; the S4 job is not a playback scheduler.

The compositor owns its linear RGBA16F image. The offscreen diagnostic imports
it through `QRhiTexture::createFrom`; the wrapper dies before the borrowed
Vulkan image or device. Readiness is synchronous in that diagnostic, and in
the viewer below. The sRGB display encode does not establish color accuracy;
ADR-0006 must define the SDR/HDR policy.

## Implementation and consequences

- [Viewer handoff](0005-viewer.md)
- [Producer admission](0005-producers.md)
- [Alternatives and acceptance](0005-alternatives.md)
