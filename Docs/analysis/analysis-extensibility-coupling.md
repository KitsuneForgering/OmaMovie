## 5. Riscos de Acoplamento Indevido

| Risco | Status | Mitigação |
|---|---|---|
| Timeline vazar detalhes GPU | ✅ Evitado | Timeline pura, sem Vulkan/Qt/FFmpeg. Gera apenas dados (RenderGraph via evaluation). |
| Media vazar p/ compositor | ✅ Controlado | Só via VideoFrame handle + contrato. Compositor não conhece FFmpeg. |
| Qt vazar p/ libs | ✅ Evitado | Qt só em apps/omamovie. Bridge bem isolado (PreviewItem). |
| Effects "hardcoded" em múltiplos lugares | ⚠ Parcial | Effects definidas em timeline ([effects.hpp](libs/timeline/include/oma/timeline/effects.hpp)), mapeadas p/ compositor ([Layer.looks/sharpness](libs/compositor/include/oma/compositor/render_graph.hpp#L157–164)), UI lê definições. Há mapeamento app↔libs (evaluation/build_viewer_frame) — coordenado mas precisa manter sincronizado ao adicionar efeito. |
| Project acoplado a timeline mas não a project-ir (atual) | ⚠ Inconsistente c/ target | [project CMake](libs/project/CMakeLists.txt#L8) depende timeline+base (não project-ir). Contradiz design target ([cmake/OmaDependencyGraph.cmake](cmake/OmaDependencyGraph.cmake#L27): prevê project-ir). Se criar project-ir/importers, project deve migrar p/ depender também de project-ir conforme target? Ou atualizar target? |

[Return to analysis-extensibility.md](analysis-extensibility.md)
