## 6. Plano de Validação (Observacional, sem mudanças agora)

Fase 1: **Reproduzir + discriminar**
- Rodar `OMA_PREVIEW_HARDWARE=0 OMA_PREVIEW_SCALE=1 make run-gui` (SW) — anotar flicker (intensidade/frequência)
- Rodar `OMA_PREVIEW_HARDWARE=1 OMA_PREVIEW_SCALE=1 make run-gui` (HW) — comparar
- Se só HW flickera → fortemente sugere H3 (foreign). Se ambos → outros fatores.

Fase 2: **Sync validation**
- `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true OMA_PREVIEW_HARDWARE=1 make run-gui` — capturar logs (sem screenshot). Procurar: layout errors, foreign queue family warnings, image usage.

Fase 3: **Timings**
- `build/release/omamovie --m4-audit /path/video.mp4 60` (HW e SW) — ver `interval_ms` spread: p95-p50 indica irregularidade.

Fase 4: **Ambiente**
- Testar fora Quickshell (direto Hyprland) se possível.
- Testar com janela fixa (não redimensionar) vs redimensionando.
- Testar cena estática vs movimento intenso.

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
