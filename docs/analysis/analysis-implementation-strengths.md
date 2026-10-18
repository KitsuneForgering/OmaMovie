## 7. Pontos Fortes da Implementação

1. **Invariantes rígidas aplicadas no código**: grafo de deps forçado em CMake; tempo sempre racional com overflow check; fronteiras bem definidas (sem vazamento Qt/Vulkan p/ libs além do estritamente necessário).
2. **Sync GPU correto e explícito**: contrato de semaphores por frame, admission gate p/ threads concorrentes com Qt, lock recursivo de queue — evita readback acidental e reduz race conditions (fundamental p/ Wayland/Vulkan).
3. **HW accel pragmático**: VA-API→DMA-BUF importado pelo próprio app (contorna leak FFmpeg 9.0.1), detecção runtime com fallback SW obrigatório, logging de downgrade.
4. **Modelo puro e testável**: timeline sem dependências pesadas, RenderGraph puro dados (testável sem GPU), CpuCompositor como referência. Muitos testes Cest por módulo.
5. **Evolução segura de projeto**: migrações encadeadas, recusa versão mais nova, preservação de unknown fields top-level, save atômico com fsync, bounds/validações p/ input não confiável ([ADR-0007](../adr/0007-project-format.md), [§18](CLAUDE.md#18-parser-safety)).
6. **Extensibilidade já pensada**: effects com definitions estáticas + stack ordenado por stages ([ADR-0016](../adr/0016-video-effects.md)), TimeMap segmentado ([ADR-0013](../adr/0013-clip-time-maps.md)), comandos undo/redo por operação (não ad-hoc).

[Return to analysis-implementation.md](analysis-implementation.md)
