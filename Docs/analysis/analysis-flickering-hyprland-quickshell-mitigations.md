## 5. Mitigações Prováveis (baseadas em arquitetura existente)

| Mitigação | Código-alvo | Efeito esperado | Viabilidade |
|---|---|---|---|
| **M1. Garantir acquire_foreign/release_foreign no compositor p/ frames DMA-BUF** | [`VulkanCompositor`](libs/compositor/src) — ao ler `LayerInput.frame` com imagens foreign | Corrige hazard entre drivers (H3). Crítico se HW path causa flicker. | **Alta** — já existem helpers ([commands.hpp](libs/gpu/include/oma/gpu/commands.hpp#L50–55)), precisa identificar quando frame é foreign. |
| **M2. Evitar reescrita prematura de `display_`** | [`VulkanCompositor::encode_display`](libs/compositor/src) / [`PreviewItem`](apps/omamovie/src/preview_item.cpp) — considerar double-buffer p/ display image (ou garantir Qt consumiu antes overwrite) | Evita tearing/flicker se Qt ainda lendo frame anterior (H2). Hoje mesma imagem. | Média — aumenta uso VRAM (display0/display1), mas simples. Alternativa: usar timeline semaphore entre encode e consumo? Qt não expõe semaphore fácil via QRhi. |
| **M3. Forçar present mode FIFO (vsync) ou detectar** | Qt RHI Vulkan — via env `QVulkanInstance`/settings? Ou documentar uso. Nem sempre controlável via env simples. | Reduz tearing (H1). Pode aumentar latência. | Baixa (pouco controle direto) — geralmente Hyprland+Qt escolhem razoável. |
| **M4. Tornar troca de textura menos disruptiva em scale change** | [`PreviewItem::updatePaintNode`](apps/omamovie/src/preview_item.cpp#L246–278) — evitar recriar textura toda vez se size igual? Ou transicionar suavemente? | Reduz flicker em auto-scale (H7). | Média — refator pequeno. |
| **M5. Ajustar timing de admission** | [`Device::admit`](libs/gpu/src/device.cpp) / bridge Qt — revisar quando fecha/abre admission (evitar hold longo) | Melhora frame pacing (H6). | Média — precisa cuidado p/ não quebrar swapchain drain. |
| **M6. Isolar foreign handling** | Adicionar flag `foreign` em `GpuImages`/frames e tratar no compositor | Geral (H3). | Alta — arquitetura já prevê foreign ([ADR-0004 amendment](../adr/0004-gpu-frames-sync-decode-policy.md#amendment-2026-10-06-omamovie-imports-va-api-surfaces-itself)). |

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
