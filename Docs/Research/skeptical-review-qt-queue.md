## Q1: Qt queue integration

Qt's tagged 6.11.2 Vulkan RHI retrieves its graphics queue with `vkGetDeviceQueue` (Q3).
Khronos requires zero creation flags for that call (K1); flagged queues require
`vkGetDeviceQueue2`. The current OmaMovie device enables internally synchronized queue flags
when supported. Thus the ordinary imported-device route has a **source-level mismatch**.
A patched installed Qt or verified native queue injection could change this conclusion.

No Qt runtime reproducer ran here. S4 must check the installed code path and choose a valid
retrieval and submission protocol. Alternatives: zero-flag queues with coordinated
render-thread submission; controlled rendering; supported queue injection; separate-device
external-memory import. A mutex used only by OmaMovie/FFmpeg does not synchronize Qt.
Internal queue synchronization also leaves image barriers, layouts, readiness and reuse
obligations intact. ADR-0005 must resolve these before claiming preview integration complete.

**Implementation follow-up (2026-10-03):** [S4](../spikes/S4-qt-shared-device.md) ran
the installed Qt 6.11.2 with zero-flag queues and verified matching device/queue handles,
borrowed RGBA16F image sampling, 60 sequential offscreen frames across two sizes,
diagnostic pixel checks and zero reported Vulkan validation errors. This narrows Q1 to
the concurrent submission and visible presentation protocol; the study above remains
the record of what was known before the runtime experiment.
The historical visible S4 run presented a static three-source scene; its
stable-frame contention and post-drain window rebuild passed validation.
`make run-gui` now opens user video through a temporary software-frame
preview. The current repeatable shared-image diagnostic is the offscreen S4
spike. M4 still needs to gate live workers across Qt swapchain lifecycle and
provide new-frame playback.

[Return to skeptical-review.md](skeptical-review.md)
