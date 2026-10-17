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

[Return to vulkan.md](vulkan.md)
