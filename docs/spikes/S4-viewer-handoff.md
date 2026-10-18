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

[Return to S4-qt-shared-device.md](S4-qt-shared-device.md)
