## 7. Risks

| Risk | Milestone | Mitigation |
|---|---|---|
| Queue synchronization between Qt, FFmpeg and the compositor | M4 | Extend S4's stable-frame lock to FFmpeg playback and pause/drain workers for every Qt swapchain lifecycle event; ADR-0005 |
| Interactive work blocking the UI through Qt's sync phase or admission (paused hardware scrub: 1.3 s p95 at 4K) | M11 | Decode on a worker without admission where nothing is submitted; publish only the latest request; [async work](implementation-plan-async-interactive.md) |
| Interaction regressions unseen between audits (5,000-clip edits: 25 → 136 ms p95) | M6/M10 | Rerun the FilmCraft-equivalent and long-form audits per milestone; bisect before redesigning |
| Driver differences in DMA-BUF/modifiers | M1–M2 | Verify mapping/device identity; log explicit copy or software+upload fallback |
| No AMD/NVIDIA hardware | M7 | Declare what was validated; look for testers early |
| CUDA handle-specific synchronization | Future | Pin API/handle semantics and test scheduler liveness |
| Slow Vulkan-Hpp compilation | M2 | Confined include + PCH |
| Scope growth across milestones | All | Keep each milestone's acceptance criteria explicit; record scope changes before implementing them |
| Qt 6 build (moc, QML registration) | M4–M6 | `find_package(Qt6)` + `AUTOMOC`, QML kept as files; validated with the M4 window (ADR-0018) |

[← Implementation plan](implementation-plan.md)
