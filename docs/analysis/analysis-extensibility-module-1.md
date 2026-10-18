### 2.1 Timeline (`libs/timeline`) — Extensibilidade alta, bem desenhada

Pontos reais:
- **Comandos (undo/redo)**: [`Editor`](libs/timeline/include/oma/timeline/editor.hpp) executa comandos via mutation; [`edit.hpp`](libs/timeline/include/oma/timeline/edit.hpp) define operações. Adicionar nova operação = criar função editora + comando aplicável/reversível. Cada edição passa por validação (`Timeline::validate()`) — garante invariantes.
- **Tipos de clip/track**: [`TrackKind`](libs/timeline/include/oma/timeline/model.hpp#L185–190): `Video, Audio, Caption`. Modelo já prevê `Caption` (ADR-0017). Extender com novo tipo requer cuidado com avaliação/render (timeline não conhece GPU).
- **TimeMap**: [`TimeMap`](libs/timeline/include/oma/timeline/model.hpp#L89–182) segmentado (`Linear, Freeze, Ramp`) — preparado p/ speed ramps/reverse ([ADR-0013](../adr/0013-clip-time-maps.md)). Extensão: novos tipos de segmento precisam manter monotonicidade, `media_offset/extent/slice/reversed` corretos.
- **Effects (stack por-clip)**: [`effects.hpp`](libs/timeline/include/oma/timeline/effects.hpp) define `EffectDefinition` estático (ID, stage, params). Adicionar efeito = nova entrada na tabela + renderer correspondente (compositor). Stack ordenado por [`EffectStage`](libs/timeline/include/oma/timeline/effects.hpp#L23–27): `Detail`, `Look`. Máx 16 efeitos/clip, 16 params — bounds definidos. **Extensão controlada (built-in)** — não plugin API (intencional).

**Como estender timeline com segurança:**
1. Não adicionar campos "ad-hoc" em QML; passar por modelo + comandos.
2. Toda nova operação → `apply/revert` + validação.
3. Preservar não-destrutividade (clips referenciam media + ranges).
4. IDs estáveis (não baseados em posição).

**DX:** Bom. Modelo puro facilita unit tests. Comandos bem encapsulados.

[Return to analysis-extensibility-modules.md](analysis-extensibility-modules.md)
