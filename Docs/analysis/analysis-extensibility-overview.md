## 1. Panorama Geral

Extensibilidade foi pensada com disciplina: evitar plugins prematuros (não-goal em [CLAUDE.md §2](CLAUDE.md#2-scope)), manter libs puras, usar interfaces claras e preservar compatibilidade (migrações + unknown fields). O código atual permite extensão controlada e segura; pontos de extensão reais existem (timeline commands/effects, compositor/nodes, media backends, project schema) mas **project-ir/importers ainda não existem fisicamente** sob `libs/` (são target em layout/design).

[Return to analysis-extensibility.md](analysis-extensibility.md)
