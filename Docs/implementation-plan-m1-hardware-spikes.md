### M1 — Hardware spikes

Code in `tools/spikes/`, disposable; whatever proves its value moves to the libs in M2/M3.
Results in `Docs/spikes/Sn-<name>.md` with numbers, driver versions and a conclusion.

| Spike | Question | Done when | Feeds |
|---|---|---|---|
| **S1** Inventory ✅ | Which codecs/profiles does the Iris Xe decode and encode through Vulkan Video, VA-API and QSV? | Codec × API × bit depth table — **done**, see `Docs/spikes/S1-hardware-inventory.md`: VA-API is the default decode path; Vulkan Video is behind `ANV_DEBUG` on TigerLake; Vulkan compute codecs (ProRes, FFv1) work without flags | ADR-0004 |
| **S2** FFmpeg on OmaMovie's device ✅ | Does the installed FFmpeg accept a `VkDevice` created with Vulkan-Hpp and deliver `AVVkFrame` without readback? How does queue locking work? | H.264/HEVC/AV1 decoded into `AVVkFrame`, no explicit host transfer reported in the application mapping path — **prototype done; follow-up validation pending**, see `Docs/spikes/S2-ffmpeg-own-device.md`: VA-API → DMA-BUF → Vulkan on our device, exact luma, no `AV_HWFRAME_MAP_DIRECT`; queues via `VK_KHR_internally_synchronized_queues` | ADR-0004 |
| **S3** Decode/encode paths ✅ | VA-API vs. QSV (vs. flagged Vulkan Video, for reference) per codec on the Iris Xe; FFv1 vs. ProRes for proxies | Frame time, CPU and power per codec; written selection policy — **decode done** (2026-10-04), see `Docs/spikes/S3-decode-paths.md`: QSV adds nothing over VA-API (keep ADR-0004 order); hardware decode saves 5–6 cores at 1080p60, not throughput; ProRes (intra, CPU-encoded: `prores_ks_vulkan` output is invalid at 1080p) is the proxy candidate, FFv1 Vulkan decode runs at 19 fps. Power not measured (RAPL root-only); export encode paths moved to M7 | ADR-0004 |
| **S4** Qt Quick on the same device ✅ | Can Qt Quick present a compositor image on the same device without a render-path readback? | The repeatable offscreen diagnostic checks identical physical/logical device and queue, borrowed compositor-image pixels and 60 frames with no validation errors. A prior static visible experiment passed bounded queue contention. Its test-pattern window was replaced after user feedback: `make run-gui` now opens real videos with a temporary software-frame upload. Production on-screen GPU handoff belongs to M4. See [S4](spikes/S4-qt-shared-device.md) | ADR-0005 |
| **S5** Minimal compositing ✅ | 2 videos + 1 image with transform/crop/opacity and YUV→RGB | 1080p60 frame time measured and recorded — **done** (2026-10-04), see `Docs/spikes/S5-minimal-compositing.md`: needed RGB image support (planar GBR from the decoder); sequential decode + composite p50 8.2–8.3 ms with VA-API (3–4 of 600 frames over 16.67 ms), 9.1–9.3 ms software with a tail to 22 ms; no presentation | M3 |
| **S6** libplacebo ✅ | Does it operate on OmaMovie's images/device without copies? Quality and cost? | Decision "use libplacebo for color/scaling/LUT or our own shaders" — **done** (2026-10-04), see `Docs/spikes/S6-libplacebo.md`: works on our device and images without copies (zero-flag queues); our shaders kept (display encode within 1 level of the formula vs 4; equal or lower cost); libplacebo is the candidate for HDR tone mapping and export scaling. Found and fixed three `gpu`/`media` defects on the way (sync-queue extension on zero-flag devices broke VA-API mapping; swapchain without surface; per-plane VA-API image formats) | ADR-0006 |

S7 (NVIDIA path) and S8 (AMD) need other hardware; they are scheduled for when a machine is available (§6).

**Done when**: ADR-0004 evidence limits are recorded, and ADR-0005/0006 are written after S4/S6 pass.

**Status (2026-10-04)**: S1–S6 done on the Iris Xe; ADR-0004/0005/0006 written. S7 (NVIDIA) and S8 (AMD) wait for hardware, so M1 is complete for Intel only. Power per decode path was not measured (RAPL needs root).
See [validation protocols](Research/skeptical-review.md#validation-protocols); S1/S2 alone do not complete M1.

**Research**: [hardware-strategy](Research/hardware-strategy.md), [vulkan](Research/vulkan.md).

[← Implementation plan](implementation-plan.md)
