## 2. Hipóteses Prováveis p/ Flickering (Hyprland + Quickshell)

Flickering em Wayland + Vulkan + Qt RHI geralmente vem de **sincronização entre compositor Wayland (Hyprland), Qt RHI e apresentação** — não de "GPU→CPU readback" (evitado aqui). Principais hipóteses:

### H1. Present mode / vsync / tearing
- Hyprland usa Wayland, apresentação depende de `vkQueuePresentKHR` + swapchain. Modo (IMMEDIATE vs MAILBOX vs FIFO) afeta tearing/flicker. Qt escolhe present mode via RHI — pode conflitar com preferências Hyprland (tearing vsync).
- Se Qt usa IMMEDIATE sem sync adequado, pode haver tearing visível; se há desalinhamento frame pacing, aparece como flicker intermitente.

### H2. Sincronização entre encode_display e amostragem Qt
- `encode_display()` escreve `display_` e retorna quando GPU terminou. Qt amostra via `createFrom` apontando p/ mesma `VkImage` (handle).
- Ordem na mesma queue está garantida (próxima encode espera trabalho anterior, sampling vem depois). Mas **layout** é `VK_IMAGE_LAYOUT_GENERAL` em ambos — correto p/ sampling? Depende de usage flags. Também Qt pode ler fora do intervalo "seguro" se não há semaphore entre Qt presentation e nosso próximo write.
- Lock recursivo da queue garante serialização entre nosso submit e submits Qt **dentro do frame window**; porém Qt faz present fora em alguns paths? ([ADR-0005](../adr/0005-qt-vulkan-interop.md#admission-for-producers-on-other-threads-2026-10-05): "Qt does not always pair `afterRendering` with `afterFrameEnd`").
- Screenshot (`QQuickWindow::grabWindow`) causa hazards conhecidos ([ADR-0005](../adr/0005-qt-vulkan-interop.md#implementation-in-the-viewer-2026-10-03): "reports three swapchain hazards whether or not the viewer composites").

### H3. DMA-BUF external memory / foreign queue family
- Imagens importadas VA-API são `foreign` ([resources.hpp](libs/gpu/include/oma/gpu/resources.hpp#L122–140), ADR-0004 amendment). Antes de ler (GPU), precisa `acquire_foreign` (transição p/ usar em outra queue family). Depois, `release_foreign`.
- Se compositor/Vulkan backend não faz `acquire_foreign/release_foreign` **exatamente** quando lê imagens DMA-BUF importadas, pode haver hazard de memória entre drivers (Mesa VA-API producer vs Vulkan consumer). Resulta em flicker/artefatos (conteúdo antigo ou parcialmente atualizado).
- Verificar: compositing lê `LayerInput.frame` que é GPU frame (DMA-BUF)? Em hardware path, `VideoFrame` guarda `DmaBufImage`s — backend precisa tratar como foreign.

### H4. Hyprland + Quickshell composição em camadas
- Quickshell roda como shell/overlay (Wayland layer shell) ou dentro de Hyprland. Compositor aninhado: Hyprland compõe superfície Qt + overlays.
- Se há **duplo composition** (app desenha + Hyprland redesenha), ou mismatch de alpha/blending, pode parecer flicker (especialmente em redimensionamento, animações, ou com vsync descoordenado).
- Também frame pacing: Hyprland tenta 60/120Hz; Qt RHI pode apresentar em ritmo ligeiramente diferente entre frames → intermitente.

### H5. Swapchain recreation / resize / device loss window
- Qt recria swapchain em resize. Bridge fecha admission, drena ([ADR-0005](../adr/0005-qt-vulkan-interop.md#admission-for-producers-on-other-threads-2026-10-05)). Mas entre `close_admission()` e reabertura, ou se `vkDeviceWaitIdle` chamado por Qt conflita com submits pendentes, pode haver um frame "vazio" ou flicker.
- Device loss tratado como final ([device.hpp:118–125](libs/gpu/include/oma/gpu/device.hpp#L118–L125)) — preview para; se ocorrer transitoriamente (raro), visível como congelamento + flicker ao voltar.

### H6. Decode thread admission timing
- Decoder admitido por frame (`admit()` com cancelled). Se `admit()` espera muito quando gate fechado (Qt swapchain change), pode atrasar próximo frame → scheduling irregular → perceived flicker.
- Com lock recursivo da queue: render thread reentra, outros threads esperam. Timing errado pode criar bursty submits.

### H7. Preview scale / adaptive quality mudando dinamicamente
- `OMA_PREVIEW_SCALE`/auto: quando cai p/ 0.5 ([session.cpp:3405–3420](apps/omamovie/src/session.cpp#L3405–L3420)), escala muda repentinamente. Se resize de textura Qt não é suave, pode parecer flicker momentâneo. Também `compositor::scaled()` muda RenderGraph output size.
- Mudança de `wrapped_` em PreviewItem ([preview_item.cpp:246–278](apps/omamovie/src/preview_item.cpp#L246–L278)): quando `wrapped_ != display_`, recria QRhiTexture + QSGTexture. Isso ocorre em resize/display change — se acontece em meio a playback, visível.

### H8. Layout transitions / image ownership
- `display_` é imagem do compositor (própria). Qt cria wrapper sem copiar. `VK_IMAGE_LAYOUT_GENERAL` usado. Em alguns drivers, sampling de image que acabou de ser escrita requer barrier correta — mas tudo na mesma queue + ordem submit deve cobrir. Porém se Qt RHI assume layout diferente p/ sampling...
- `foreign` images: ao usar DMA-BUF importado, precisa acquire/release foreign. Se compositor renderiza camadas com frames DMA-BUF HW, **deve** fazer acquire antes uso, release depois. Verificar implementação VulkanCompositor p/ isso (não inspecionado em detalhe aqui).

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
