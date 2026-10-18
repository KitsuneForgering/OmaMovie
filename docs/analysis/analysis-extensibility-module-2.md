### 2.2 Compositor & RenderGraph (`libs/compositor`) — Extensibilidade média-alta

Pontos reais:
- **RenderGraph puro**: [`RenderGraph`](libs/compositor/include/oma/compositor/render_graph.hpp#L179–184) é dado declarativo (layers bottom-first, [`Layer`](libs/compositor/include/oma/compositor/render_graph.hpp#L137–177) com fit/crop/transform/opacity/blend, [`looks`](libs/compositor/include/oma/compositor/render_graph.hpp#L157–159) ordenados, [`sharpness`](libs/compositor/include/oma/compositor/render_graph.hpp#L161–164), [`reveal`](libs/compositor/include/oma/compositor/render_graph.hpp#L165–167), [`grade`](libs/compositor/include/oma/compositor/render_graph.hpp#L169–170)). Testável sem GPU.
- **Nó/effects de render**: Efeitos mapeados p/ `Layer` (looks compostos, detail via sharpness). Renderer atual (Vulkan) implementa isso via shaders embutidos ([compositor CMake](libs/compositor/CMakeLists.txt#L5–L37): GLSL→SPIR-V compilados em build). Adicionar novo efeito requer: estender `Layer` (ou representar como look/detail/grading), atualizar shaders, atualizar CpuCompositor (referência), atualizar validação (`validate()`), manter compatibilidade com timeline effects.
- **Compute backend**: Interface [`ComputeBackend`](libs/compositor) não vista explicitamente em headers listados, mas mencionado em [CLAUDE.md §9.3](CLAUDE.md#93-compute): `class ComputeBackend; VulkanComputeBackend, CudaBackend, CpuBackend`. Se interface bem definida, adicionar backend é possível. **Interface-first** ainda precisa ser consolidada.

**Extensibilidade render:** Bom p/ efeitos built-in. Evita abstração excessiva (não node-graph completo). Ruim p/ plugins dinâmicos (não é objetivo). Para adicionar novo efeito: melhor caminho é seguir ADR-0016 (definição estática + stage + renderer).

**Risco:** mudar `RenderGraph` quebra compatibilidade entre timeline→render (precisa coordenar com evaluation).

[Return to analysis-extensibility-modules.md](analysis-extensibility-modules.md)
