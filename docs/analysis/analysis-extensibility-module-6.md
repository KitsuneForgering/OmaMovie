### 2.6 UI/Bridge (`apps/omamovie`) — Extensibilidade contextual

- **Effects discovery**: UI precisa descobrir efeitos disponíveis. Hoje vem de [`effect_definitions()`](libs/timeline/include/oma/timeline/effects.hpp#L71–72) (tabela estática em timeline). Session expõe lista p/ QML? ([Session](apps/omamovie/src/session.hpp) provavelmente). Para adicionar efeito: atualizar tabela + renderer + UI drawer. Acoplamento mínimo (só lê definições).
- **Inspector contextual**: UI muda conforme seleção ([ui-design.md](../ui-design.md), [CLAUDE.md §11](CLAUDE.md#11-ui-qt-quick--qml)). Extensível via delegates QML por tipo de seleção.
- **Shortcuts centralizados**: recomendado ([§11.2](CLAUDE.md#112-shortcuts)) — verificar se já existe action system central.
- **Bridge sem vazar GPU**: PreviewItem encapsula Vulkan/Qt RHI completamente — bom isolamento.

[Return to analysis-extensibility-modules.md](analysis-extensibility-modules.md)
