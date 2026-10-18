## 6. Sugestões Concretas p/ Melhorar Extensibilidade

| Sugestão | Prioridade | Justificativa |
|---|---|---|
| **1. Criar `libs/project-ir` + esqueleto `libs/importers`** | Alta | Materializar design existente ([§16](CLAUDE.md#16-interoperability-and-reverse-engineering)). Permite validar fluxo `Importer→ProjectIR→converter` sem adivinhar. Começar com formato acessível (OTIO JSON ou FCPXML simples). |
| **2. Alinhar grafo CMake/project com target**: incluir `project-ir` como dep de `project` quando existir | Alta | Eliminar inconsistência entre [target](cmake/OmaDependencyGraph.cmake#L27) e [atual](libs/project/CMakeLists.txt#L8). Evitar divergência futura. |
| **3. Consolidadar interface `ComputeBackend`** | Média | [§9.3](CLAUDE.md#93-compute) menciona interface mas não encontrada em headers inspecionados. Tornar explícita (header público) facilita adicionar CUDA/Vulkan Compute de forma limpa. |
| **4. Melhorar preservação unknown fields por objeto** | Média | Permite round-trip mais robusto entre versões. Útil p/ importers e evolução futura. Requer cuidado com alocação/DTOs. |
| **5. Centralizar mapeamento timeline↔render (effects)** | Média | Reduz duplicação entre timeline effects, compositor Layer, evaluation (build_viewer_frame). Poderia ter um adaptador em `libs/playback` ou util compartilhado? Playback já conhece timeline+media+compositor — talvez natural. |
| **6. Documentar DX p/ adicionar efeito** (checklist) | Baixa | Apesar de bom, checklist explícito ("update effects.hpp, add shader if needed, update CpuCompositor, update validation, update UI") evitaria esquecimentos. |

[Return to analysis-extensibility.md](analysis-extensibility.md)
