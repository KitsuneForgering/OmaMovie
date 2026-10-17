## 7. Conclusão (Flickering)

**Causa mais plausível:** H3 — `acquire_foreign/release_foreign` não sendo chamado no compositor ao usar frames DMA-BUF importados (hardware path). Isso é comum quando se adiciona foreign images sem propagar p/ todos consumidores.

**Segundo lugar:** H2 — `display_` único reescrito sem double-buffering entre encode e consumo Qt. Menos provável causar flicker forte, mas pode causar tearing em alguns casos.

**Terceiro:** H4 (Quickshell aninhado) — ambiental, mas alinhado com relato.

**Recomendação imediata (investigativa):** antes de mudar código, fazer Fase 1 discriminando SW/HW. Se HW-only, **priorizar M1** (verificar/completar foreign acquire/release no VulkanCompositor). Também rodar validation layer com sync p/ pegar warnings.

Arquitetura já tem peças certas (admission gate, shared device, sem readback). Falta provavelmente apenas completar tratamento de imagens foreign no caminho compositor→preview.

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
