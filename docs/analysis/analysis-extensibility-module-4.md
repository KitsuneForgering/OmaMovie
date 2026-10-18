### 2.4 Project, ProjectIR, Importers — Extensibilidade projetada, mas **não implementada**

**Estado atual:**
- `libs/project` **existe** e implementa formato nativo com migrações, serialização DTO↔modelo, save atômico, unknown fields top-level ([ADR-0007](../adr/0007-project-format.md), [project](libs/project)).
- `libs/project-ir` **não existe** sob `libs/` (apenas previsto em [§5.1](CLAUDE.md#51-layout), [cmake/OmaDependencyGraph.cmake](cmake/OmaDependencyGraph.cmake#L26)).
- `libs/importers` **não existe** (previsto).

**Pontos de extensão projetados (não codificados ainda):**
- **ProjectIR neutro**: `External project -> <Format>Importer -> ProjectIR -> converter -> OmaMovie project`. Deve ser independente de modelo interno ([§16.2](CLAUDE.md#162-projectir)). Extender p/ novo formato = novo importer.
- **Preservação dados desconhecidos**: tipos `UnknownEffect`, `UnsupportedEffect`, `OpaqueExternalObject` mencionados ([§16.3](CLAUDE.md#163-preserving-external-data)) — ainda não vistos em headers.
- **Suporte por formato+versão** (0–5) + import report ([§16.4–§16.5](CLAUDE.md#164-import-report), [§16.5](CLAUDE.md#165-support-levels)), documentação `docs/formats/<format>.md` ([§16.6](CLAUDE.md#166-reverse-engineering-rules)).
- `oma-project` CLI ([§16.7](CLAUDE.md#167-oma-project-cli), [tools/oma-project](tools/oma-project)) existe? Verificar: `tools/oma-project/` existe (mencionado). Usa libs/project.

**Avaliação:** Design excelente (separação clara). Implementação ainda faltante — **maior lacuna de extensibilidade atual**. Adicionar importer hoje exigiria criar project-ir + converter, respeitando fronteiras (importer não depende de modelo interno).

**Recomendação:** criar `libs/project-ir` primeiro (interfaces mínimas), depois `libs/importers` com pelo menos 1 formato acessível (OTIO/FCPXML/EDL) para validar design.

[Return to analysis-extensibility-modules.md](analysis-extensibility-modules.md)
