## 2. Matriz: Fortes x Fracos

| Dimensão | Fortes | Fracos/Lacunas |
|---|---|---|
| **Implementação** | Fronteiras rígidas (CMake dep graph), tempo racional, sync GPU explícito, modelo puro testável, HW accel com fallback obrigatório. | project-ir/importers ausentes (só target), `libs/project` não wired à app, export ausente (M7), device loss recovery não automático, leak FFmpeg suprimido. |
| **Extensibilidade** | Timeline commands/undo-redo claros, effects built-in bem estruturados, RenderGraph puro dados, contrato VideoFrame claro p/ backends HW. | Extensibilidade de importers **teórica** (falta materialização project-ir/importers). Gasto de deps CMake vs target (project não depende project-ir). Interface ComputeBackend não consolidada. Unknown fields só top-level. Mapeamento timeline↔render um pouco disperso. |
| **Integração/Preview** | Shared device sem readback, admission gate, lock recursivo queue, foreign image support existente, variáveis debug (`OMA_PREVIEW_*`), métricas m4-audit. | Possível tratamento incompleto de `acquire_foreign/release_foreign` no compositor p/ frames DMA-BUF (suspeito p/ flicker HW). `display_` único sem double-buffering. Ambiente Quickshell aninhado pode interagir mal com present/sync. |

[Return to analysis-integral-consolidado.md](analysis-integral-consolidado.md)
