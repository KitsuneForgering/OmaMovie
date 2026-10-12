## 4. Fluxo de Dados (Decode → GPU → Compositor → Preview)

```
Mídia comprimida (arquivo)
→ demux (media) → AVPacket/AVStream
→ decode (media::VideoDecoder): tenta VA-API→DMA-BUF (import próprio OmaMovie, [ADR-0004 amendment](../adr/0004-gpu-frames-sync-decode-policy.md#amendment-2026-10-06-omamovie-imports-va-api-surfaces-itself)), fallback Vulkan Video, fallback SW
→ media::VideoFrame (GPU: DmaBufImage(s) + timeline semaphores; CPU: planos float/RGBA8)
→ FrameSource (apps/omamovie/src/frame_source.hpp): cache/streaming de frames (history por stream, bounded budget), avança/retrocede com seek otimizado
→ build_viewer_frame ([viewer_frame.hpp](apps/omamovie/src/viewer_frame.hpp#L32–47)): timeline.evaluate → RenderGraph (dados puros)
→ compositor::VulkanCompositor.render(): traduz RenderGraph → comandos Vulkan, usa timeline semaphores p/ sync frames GPU
→ output() image RGBA16F (linear, premultiplied)
→ encode_display(Transfer::Srgb) → image RGBA8 sampleável
→ PreviewItem ([preview_item.cpp](apps/omamovie/src/preview_item.cpp#L224)): QRhiTexture::createFrom(image, VK_IMAGE_LAYOUT_GENERAL) sem readback CPU ([ADR-0005](../adr/0005-qt-vulkan-interop.md#implementation-in-the-viewer-2026-10-03)), presente via Qt RHI Vulkan
```

Pontos críticos de sync: `GpuImages` com semaphores por frame ([ADR-0004](../adr/0004-gpu-frames-sync-decode-policy.md)), `Device::admit/leave/close_admission/open_admission` ([device.hpp:108–125](libs/gpu/include/oma/gpu/device.hpp#L108–L125)) — gate p/ produtores (decode thread) antes/durante swapchain changes do Qt ([ADR-0005](../adr/0005-qt-vulkan-interop.md#admission-for-producers-on-other-threads-2026-10-05)).

[Return to analysis-implementation.md](analysis-implementation.md)
