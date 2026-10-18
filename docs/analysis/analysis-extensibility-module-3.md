### 2.3 Media & HW Accel (`libs/media`, `libs/gpu`) — Extensibilidade média

Pontos reais:
- **Seleção de hwaccel**: [`VideoDecoder`](libs/media/include/oma/media/video_decoder.hpp) tenta ordem VA-API→DMA-BUF import próprio → Vulkan Video → SW ([ADR-0004](../adr/0004-gpu-frames-sync-decode-policy.md)). Para estender: adicionar novo backend (NVDEC, VA-API diferente) respeitando contrato `VideoFrame` (GpuImages + semaphores).
- **Frame abstraction**: [`VideoFrame`](libs/media) com contrato explícito (`acquire_gpu()` retorna `GpuAccess` com semaphores/timeline values). Qualquer novo backend HW **deve** cumprir esse contrato — evita vazamento de sync.
- **Interop GPU**: [`DmaBufImage`](libs/gpu/include/oma/gpu/resources.hpp#L87–140) permite importar superfícies externas (VA-API). [`acquire_foreign/release_foreign`](libs/gpu/include/oma/gpu/commands.hpp#L50–55) para imagens estrangeiras (VK_QUEUE_FAMILY_FOREIGN_EXT). Estrutura preparada p/ novos tipos de external memory.
- **Admission/sync**: [`Device::admit/leave`](libs/gpu/include/oma/gpu/device.hpp#L108–125) permite produtores concorrentes controlados. Bom ponto p/ estender com novos produtores (export, thumbnails async) sem quebrar Qt bridge.

**Extensibilidade HW:** Boa arquitetura. Porém adicionar NVDEC requer hardware + validação (S7 pendente). Contrato estrito é vantajoso (impede shortcuts).

[Return to analysis-extensibility-modules.md](analysis-extensibility-modules.md)
