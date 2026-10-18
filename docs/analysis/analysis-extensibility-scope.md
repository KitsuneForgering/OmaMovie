## 4. Extensibilidade "Real" vs "Teórica"

| Área | Real | Teórica | Notas |
|---|---|---|---|
| Timeline commands/ops | Alta | — | Já praticado (edit operations, undo/redo). Fácil adicionar novos comandos mantendo validação. |
| Video effects (built-in) | Alta | — | ADR-0016 define caminho claro (definition + stage + renderer). Não é plugin, mas extensível controlado. |
| Render nodes/shaders | Média-Alta | — | Precisa tocar Layer + shaders + CpuCompositor + timeline mapping. Coordenado mas factível. |
| Media backends HW | Média | — | Contrato VideoFrame claro. NVDEC requer validação. |
| Compute backends | Média | — | Interface mencionada mas precisa ser consolidada. |
| Importers (externos) | **Baixa (não implementado)** | Alta (design bom) | Falta project-ir + estrutura importers. Design correto, código ausente. |
| Project schema evolution | Alta | — | Muito bem feito (migrações + bounds + unknown top-level). |
| Audio filters RT | Média | — | Respeitar restrições RT é limitante mas necessário. |
| UI discovery | Alta | — | Lê definições estáticas, delegates QML — fácil estender sem vazar libs. |

[Return to analysis-extensibility.md](analysis-extensibility.md)
