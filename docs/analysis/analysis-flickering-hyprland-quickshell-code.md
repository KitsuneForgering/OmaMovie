## 3. Investigação Baseada em Código Existente

### 3.1 Pontos já validados
- S4: shared device + wrapping image funciona offscreen, 60 frames sem erros ([S4](../spikes/S4-qt-shared-device.md)).
- ADR-0005: handoff viewer implementado; admission gate testado conceitualmente.
- M4 hardware gate: passou em vários cenários com interpolated clock ([evidence 2026-10-06](../Research/evidence/2026-10-06-m4-hardware-gate.txt)) — mas testes foram com `--m4-audit` (modo audit/headless-ish) ou GUI smoke? Smoke menciona hazards só com `grabWindow`.
- Validação layer: smoke reports "no validation errors when no screenshot is taken" ([ADR-0005](../adr/0005-qt-vulkan-interop.md#implementation-in-the-viewer-2026-10-03)).

### 3.2 Pontos suspeitos não totalmente cobertos

1. **Foreign acquire/release no compositor** — não vimos explicitamente em [compositor](libs/compositor/src) (não lido todo). Se camadas usam frames HW DMA-BUF, compositor precisa `acquire_foreign` antes de binding p/ sampling/read na queue, `release_foreign` após. Ausência causa flicker/instabilidade em Hyprland (mais sensível).
2. **Semaphores entre Qt presentation e nosso próximo write** — mesma queue garante ordem de submits, mas Qt pode não sinalizar semaphore p/ nós. Usar mesma queue é suficiente? Com zero-flag queues + external sync, ordem é por submit order. Se Qt apresenta frame N e nós escrevemos display_ p/ N+1 na mesma queue **após** submit Qt, ok. Mas se display_ é reescrito antes Qt terminar de apresentar N? Qt segura até afterFrameEnd? Bridge segura queue nesse intervalo — mas display_ é imagem única (não double-buffered pelo compositor p/ preview)? Ver [compositor](libs/compositor): `encode_display` escreve na mesma imagem `display_` (retorna ponteiro p/ ela) — reescrita pode acontecer antes Qt consumir frame anterior? Depende de timing.
3. **Hyprland explicit sync** — Wayland 1.20+ suporta explicit sync (DMA-BUF sync_file). Qt RHI Vulkan deve usar isso se disponível. Mesa + Hyprland modernos suportam. Mas se Qt/Hyprland não concordam, flicker. Difícil diagnosticar sem debug.
4. **Quickshell** — se app roda dentro de Quickshell (wayland surface filho), composição pode ser diferente. Handoff menciona "flickering ao rodar o omamovie no hyprland/quickshell" — sugestivo de camada extra.

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
