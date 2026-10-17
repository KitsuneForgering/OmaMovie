## 4. Variáveis & Ferramentas p/ Diagnóstico

### 4.1 Variáveis existentes (úteis)
- `OMA_PREVIEW_HARDWARE=0|1` — forçar SW/HW. Testar ambos: HW usa DMA-BUF foreign (mais propenso a H3), SW faz upload CPU (evita foreign) — bom discriminador.
- `OMA_PREVIEW_SCALE=1` ou fração fixa — evitar auto-adapt que muda textura ([session.cpp:3370–3388](apps/omamovie/src/session.cpp#L3370–L3388)). Fixar p/ isolar.
- `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true` — já usado, confirma sem erros quando sem screenshot ([ADR-0005](../adr/0005-qt-vulkan-interop.md#implementation-in-the-viewer-2026-10-03)).
- `QT_QPA_PLATFORM=wayland` (padrão) vs `wayland-egl` vs `xcb` (não usar xcb). Testar wayland-egl às vezes ajuda.
- `QT_LOGGING_RULES="qt.qpa.*=true"` p/ debug Qt platform.
- `vblank_mode=0` / `__GL_SYNC_TO_VBLANK=0` raramente necessário, mas evitar forçar.
- `MESA_DEBUG=1`, `RADV_DEBUG=info` (AMD) ou `ANV_DEBUG=...` (Intel) — verbose driver.

### 4.2 Ferramentas diagnósticas existentes
- `--m4-audit FILE SECONDS` — timing por estágio, dropped frames, erro A/V ([CLAUDE.md §3](CLAUDE.md#3-commands)). Útil p/ ver se há irregularidade (interval_ms varia muito).
- `--m4-seek-audit FILE SEEKS` — latência first-frame.
- GUI smoke: `make run-gui RUN_GUI_SMOKE=1` com validation layer.
- S4 diagnostic: reproduzível.

### 4.3 Sugestões de testes observacionais (sem modificar código ainda)

1. **Discriminar SW vs HW**
   - `OMA_PREVIEW_HARDWARE=0 make run-gui` (SW: upload CPU, sem DMA-BUF foreign) — se flicker some muito ou some completamente → sugere H3 (foreign/DMA-BUF)
   - `OMA_PREVIEW_HARDWARE=1 make run-gui` (HW: VA-API→DMA-BUF) — comparar

2. **Fixar scale (evitar auto)**
   - `OMA_PREVIEW_SCALE=1 make run-gui` — desativa auto-adapt

3. **Validation layer com sync**
   - `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true OMA_PREVIEW_HARDWARE=1 make run-gui` — observar warnings/errors (exceto grabWindow)

4. **Testar sem Quickshell?** (se possível) — rodar direto Hyprland (não aninhado em Quickshell) p/ ver se desaparece → confirma H4

5. **Comparar resoluções/resize** — redimensionar janela repentinamente: se piora → sugere H5/H7

6. **Analisar timings** — rodar com `--m4-audit` por 60s e olhar `interval_ms` p50/p95/p99: muito espalhado sugere frame pacing irregular (H1/H6)

[Return to analysis-flickering-hyprland-quickshell.md](analysis-flickering-hyprland-quickshell.md)
