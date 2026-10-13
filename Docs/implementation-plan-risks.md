## 7. Risks

| Risk | Milestone | Mitigation |
|---|---|---|
| Queue synchronization between Qt, FFmpeg and the compositor | M4 | Extend S4's stable-frame lock to FFmpeg playback and pause/drain workers for every Qt swapchain lifecycle event; ADR-0005 |
| Driver differences in DMA-BUF/modifiers | M1–M2 | Verify mapping/device identity; log explicit copy or software+upload fallback |
| No AMD/NVIDIA hardware | M7 | Declare what was validated; look for testers early |
| CUDA handle-specific synchronization | Future | Pin API/handle semantics and test scheduler liveness |
| Slow Vulkan-Hpp compilation | M2 | Confined include + PCH |
| Scope growth across milestones | All | Keep each milestone's acceptance criteria explicit; record scope changes before implementing them |
| Qt 6 build (moc, QML registration) | M4–M6 | `find_package(Qt6)` + `AUTOMOC`, QML kept as files; validated with the M4 window (ADR-0018) |

[← Implementation plan](implementation-plan.md)
