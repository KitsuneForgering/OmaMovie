## 3. Versionamento, Migrações e Compatibilidade

**Muito forte no atual código.**

- **Timeline effects**: mudança de `filter/sharpness` → stack (v3→v4) já mapeada ([ADR-0016 implementation notes](../adr/0016-video-effects.md#implementation-notes-2026-10-07)). Migração explícita.
- **Project format**: [`format_version`](../adr/0007-project-format.md#decision) inteiro, migrações encadeadas `vN→vN+1`, recusa projeto mais novo, preserva unknown fields **top-level** apenas ([ADR-0007](../adr/0007-project-format.md#decision): "Unknown fields inside known objects… are not kept yet").
- **Effects desconhecidos**: [`unknown_effects()`](libs/timeline/include/oma/timeline/effects.hpp#L105–115) retorna lista, export recusa iniciar se houver desconhecidos ([ADR-0016](../adr/0016-video-effects.md)). UI/reporta. Bom p/ forward-compat (projeto aberto em build mais antigo parcialmente protegido).

**Lacuna:** preservação unknown fields por objeto (clip/track) — dificulta round-trip perfeito entre builds com features diferentes.

[Return to analysis-extensibility.md](analysis-extensibility.md)
