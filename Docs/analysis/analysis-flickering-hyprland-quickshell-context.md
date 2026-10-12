## 1. Contexto Atual

- Preview entrega imagem GPU ao Qt sem readback: [`PreviewItem`](apps/omamovie/src/preview_item.cpp#L224) usa `QRhiTexture::createFrom({image, VK_IMAGE_LAYOUT_GENERAL})` ([ADR-0005](../adr/0005-qt-vulkan-interop.md#implementation-in-the-viewer-2026-10-03)).
- Qt Quick roda com RHI Vulkan, compartilhando `VkDevice` do OmaMovie (same-device). Queues criadas com zero flags p/ Qt recuperar via `vkGetDeviceQueue` ([ADR-0005](../adr/0005-qt-vulkan-interop.md#decision)).
- Bridge trava queue gráfica entre `beforeFrameBegin`…`afterFrameEnd` (intervalo onde Qt também opera) — lock recursivo ([device.hpp](libs/gpu/include/oma/gpu/device.hpp#L94–97): "The lock is recursive").
- Admission gate: produtores (decode thread) só admitem quando gate aberto ([device.hpp:108–125](libs/gpu/include/oma/gpu/device.hpp#L108–125)), bridge fecha admission antes swapchain events e abre por frame ([ADR-0005](../adr/0005-qt-vulkan-interop.md#admission-for-producers-on-other-threads-2026-10-05)).
- VA-API→DMA-BUF importado pelo próprio OmaMovie ([ADR-0004 amendment](../adr/0004-gpu-frames-sync-decode-policy.md#amendment-2026-10-06-omamovie-imports-va-api-surfaces-itself)): `DmaBufImage` importa planos, imagens marcadas `foreign` (acquire/release p/ VK_QUEUE_FAMILY_FOREIGN_EXT) ([resources.hpp](libs/gpu/include/oma/gpu/resources.hpp#L87–140), [commands.hpp](libs/gpu/include/oma/gpu/commands.hpp#L50–55)).
- Display encode produz `display_` RGBA8 em `VK_IMAGE_LAYOUT_GENERAL` sampleável ([compositor.hpp](libs/compositor/include/oma/compositor/compositor.hpp#L66–90)): próxima encode espera trabalho anterior na queue; Qt sampling vem depois da encode (ordem na mesma queue).
- Variáveis: `OMA_PREVIEW_HARDWARE` força HW/SW ([preview_item.cpp](apps/omamovie/src/preview_item.cpp#L20–L60)), `OMA_PREVIEW_SCALE` controla resolução preview (auto/full/fixed fraction) ([session.cpp:3355–3420](apps/omamovie/src/session.cpp#L3355–L3420), [session.hpp:139–142](apps/omamovie/src/session.hpp#L139–L142)).
- Métricas diagnósticas: `--m4-audit`, `--m4-seek-audit`, `--export-audit` ([CLAUDE.md §3](CLAUDE.md#3-commands)) + timings em PreviewItem ([preview_item.hpp](apps/omamovie/src/preview_item.hpp#L67–94), [session onTick/timings](apps/omamovie/src/session.cpp#L3430+)).

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
