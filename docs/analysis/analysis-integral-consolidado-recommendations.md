## 4. Recomendações Prioritizadas

### Curto prazo (investigativo + correções rápidas)

| # | Ação | Justificativa | Onde |
|---|---|---|---|
| **P0.1** | **Investigar flicker: SW vs HW** | Discriminar causa (H3). Se HW-only → quase certo foreign acquire/release faltando. | Rodar com `OMA_PREVIEW_HARDWARE=0` vs `1`, fixar `OMA_PREVIEW_SCALE=1`, validation layer `VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true`. Ver doc-C Fase 1–4. |
| **P0.2** | **Completar foreign acquire/release no compositor (se HW-only)** | Suspeita #1 p/ flickering. Helpers já existem ([commands.hpp](libs/gpu/include/oma/gpu/commands.hpp#L50–55)), arquitetura prevê. | `libs/compositor/src/vulkan_compositor*` — ao ler LayerInput com frames DMA-BUF foreign, fazer acquire antes uso, release após. |
| **P0.3** | **Criar `libs/project-ir` (mínimo)** | Materializar design ([§16](CLAUDE.md#16-interoperability-and-reverse-engineering)). Elimina "extensibilidade teórica". | Criar `libs/project-ir/include/oma/project_ir/*`, `src/*`, CMakeLists, adicionar a [cmake/OmaDependencyGraph.cmake](cmake/OmaDependencyGraph.cmake) (`ALLOWED_DEPS_project-ir = base`). |
| **P0.4** | **Alinhar grafo CMake: project depende project-ir** | Consistência entre target e atual. Evitar divergência. | Atualizar [libs/project/CMakeLists.txt](libs/project/CMakeLists.txt): `DEPS timeline project-ir base` (ou conforme target). Atualizar grafo se necessário. |

### Médio prazo (M7 + materializar extensibilidade)

| # | Ação | Justificativa | Onde |
|---|---|---|---|
| **M1** | **Conectar `libs/project` à app** | Fechar M7 parcialmente: carregar/salvar projeto nativo via UI (Session/Editor). Hoje só em memória. | `apps/omamovie/src/session.*`, `editor` bridge — salvar/carregar `.omamovie`, integrar com undo/history. |
| **M2** | **Implementar export** | Fecha pipeline GPU→encode→mux (M7). Precisa respeitar admission gate (não conflitar com Qt swapchain), usar formatos definidos. | Novo módulo/tools ou em app? `tools/bench` was a proposed tool; export pode ser `apps/omamovie` action + `libs/media` encode/mux. Ver [implementation-plan M7](../implementation-plan.md#m7--project--export). |
| **M3** | **Esqueleto `libs/importers` + 1 formato acessível** | Validar fluxo `Importer→ProjectIR→converter→OmaMovie project`. OTIO JSON ou FCPXML simples são bons pontos iniciais (texto/estruturado). | `libs/importers/<format>/`, `tools/oma-project` já existe ([§16.7](CLAUDE.md#167-oma-project-cli)) — pode estender. |
| **M4** | **Consolidar `ComputeBackend` interface** | [§9.3](CLAUDE.md#93-compute) prevê interface clara. Facilita adicionar backends sem acoplamento. | Criar header público em compositor/gpu? Ver arquitetura: "Effects do not depend directly on CUDA. An effect declares what it needs; the backend executes it." |
| **M5** | **Considerar double-buffer p/ `display_` preview** | Reduz chance de reescrita prematura (H2). Baixo custo (2x imagem RGBA8 preview). | [`VulkanCompositor`](libs/compositor/src): manter `display0/display1`, alternar; PreviewItem pega último completo. |

### Longo prazo (refinamentos)

| # | Ação | Justificativa |
|---|---|---|
| **L1** | **Preservação unknown fields por objeto** ([ADR-0007](../adr/0007-project-format.md#decision)) | Round-trip mais robusto p/ importers e evolução. |
| **L2** | **Centralizar mapeamento timeline↔render (effects)** | Reduz duplicação, facilita adicionar efeitos com menos erros. |
| **L3** | **DX checklist p/ adicionar efeito** | Documentar passos explícitos (evita esquecimentos). |
| **L4** | **NVDEC/AMD (S7/S8)** quando HW disponível | Ampliar matriz validada. |
| **L5** | **Device loss recovery automático (se necessário)** | Só vale a pena se ocorrer em uso real; complexo c/ Qt shared device. |

[Return to analysis-integral-consolidado.md](analysis-integral-consolidado.md)
