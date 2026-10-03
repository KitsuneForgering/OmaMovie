# S4 — Qt Quick on OmaMovie's Vulkan device

**2026-10-03, reference machine:** Intel Iris Xe (TGL GT2), Mesa 26.2.2,
Qt 6.11.2, Vulkan 1.4, Wayland. The repeatable shared-image check is
[the offscreen diagnostic](../../tools/spikes/s4_qt_shared_device.cpp).
The runnable [GUI](../../apps/omamovie/src/main.cpp) now presents compositor
images the same way (see ADR-0005, *Implementation in the viewer*).

Run from the repository root:

```sh
make build/debug/spikes/s4_qt_shared_device
QT_QPA_PLATFORM=wayland ./build/debug/spikes/s4_qt_shared_device
make run-gui
make run-gui GUI_FILE=/path/to/video.mp4
make run-gui RUN_GUI_SMOKE=1
```

The GUI opens on Projects. Create a session and import a local video, or pass
a path with `GUI_FILE` to enter Edit directly. The editor shell has a library,
viewer and timeline with selection, temporary append/remove, silent play/pause
and seek. The smoke check opens H.264, seeks, switches to HEVC 10-bit, resizes
the window and checks that frames advance. Audio and a proper media clock
remain M4 work; the timeline's command model and persistence remain M5/M7.

## Device and queue result

The offscreen diagnostic verified matching physical device, logical device
and zero-flag graphics queue handles. Qt wrapped a borrowed RGBA16F compositor
image, checked output pixels on four frames, then passed 60 alternating
frames at two sizes with no Vulkan validation errors. Its readback exists only
to test the pixels. `QT_QPA_PLATFORM=offscreen` did not initialize
`QVulkanInstance` on this machine; the invisible `QQuickRenderControl`
worked with the Wayland plugin.

Qt 6.11.2 retrieves its imported graphics queue through `vkGetDeviceQueue`.
That call requires zero queue creation flags, so `DeviceOptions` selects
zero-flag queues for Qt while headless consumers retain their internally
synchronized queues. The Qt bridge locks the queue between
`beforeFrameBegin` and `afterFrameEnd`. Qt's swapchain creation, resize and
teardown also call `vkDeviceWaitIdle` outside those signals, so a live GPU
producer must pause and drain around those lifecycle events.

An earlier visible test-pattern experiment did wrap the compositor's image
and reported 60.0 Qt frame signals/s during static redraw, 120 concurrent
worker submissions, and zero synchronization-validation errors. That window
was replaced because it had no usable import or playback flow. Those figures
describe static redraw, not newly decoded or composited frames, and the
former visible experiment is no longer the `make run-gui` executable.

## Viewer handoff (2026-10-03)

The editor's viewer composites on Qt's render thread and Qt samples the
compositor's display image directly; `make run-gui RUN_GUI_SMOKE=1` checks that
the viewer shows composited video. Run with
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true`,
the smoke reports no validation errors when no screenshot is taken; with
screenshots, `grabWindow` reports three swapchain hazards whether or not the
viewer composites (Qt's capture path, not the handoff).

## Remaining acceptance work

The diagnostic answers S4's device-import question, but the current interactive
GUI uploads software-decoded RGBA frames into Qt textures. Production on-screen
compositor-image presentation without readback belongs to M4. M4 must add
bounded new-frame handoff, explicit GPU readiness and lifetime, FFmpeg
hardware decode sharing, pause/drain on arbitrary swapchain changes, audio
synchronization and measured end-to-end frame pacing. S6 must independently
validate SDR/HDR color. See [ADR-0005](../adr/0005-qt-vulkan-interop.md).
