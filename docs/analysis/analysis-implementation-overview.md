## 1. Visão Geral

OmaMovie é um editor de vídeo local, GPU-first, nativo para Linux/Omarchy. A arquitetura segue estritamente as invariantes de `CLAUDE.md`:
- Tempo primário com `RationalTime`/`Rational` (nunca `float`/`double` para timestamps) — [ADR-0002](../adr/0002-time-representation.md).
- Pipeline GPU-first: decode → GPU surface → Vulkan compositor → preview (sem readback no hot path) — [ADR-0004](../adr/0004-gpu-frames-sync-decode-policy.md), [ADR-0005](../adr/0005-qt-vulkan-interop.md).
- Fronteiras de módulos estritas, com grafo de dependências imposto por CMake ([cmake/OmaDependencyGraph.cmake](cmake/OmaDependencyGraph.cmake)).
- Qt **apenas em `apps/`** e código de ponte; libs não dependem de Qt ([CLAUDE.md §5.2](CLAUDE.md#52-dependency-rules-invariants)).
- Timeline é pura (sem GPU/Qt/FFmpeg) — [ADR-0003](../adr/0003-threading-and-job-system.md), [§10](CLAUDE.md#10-timeline).

[Return to analysis-implementation.md](analysis-implementation.md)
