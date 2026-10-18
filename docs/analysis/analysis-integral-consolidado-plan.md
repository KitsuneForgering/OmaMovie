## 5. Plano de Ação Imediato (sequencial prático)

**Fase 1 – Diagnóstico flicker (1–2h)**
1. Testar `OMA_PREVIEW_HARDWARE=0 OMA_PREVIEW_SCALE=1` vs `1` (HW) em Hyprland+Quickshell.
2. Com HW, rodar com `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_VALIDATION_VALIDATE_SYNC=true`.
3. Rodar `--m4-audit` HW vs SW p/ comparar `interval_ms`.
4. Se HW-only + warnings/erros relacionados a foreign/layout → **ir p/ Fase 2**.

**Fase 2 – Corrigir foreign (se confirmado)**
1. Inspecionar [`VulkanCompositor`](libs/compositor/src) p/ ver se ao samplear frames DMA-BUF faz `acquire_foreign` antes de uso em queue e `release_foreign` após.
2. Adicionar tratamento: detectar se `VideoFrame` tem imagens foreign (DmaBufImage)? Ou marcar via flag. Arquitetura já tem helpers.
3. Testar novamente com HW.

**Fase 3 – Materializar project-ir (paralelo)**
1. Criar estrutura mínima `libs/project-ir` (headers + src vazios coerentes c/ design).
2. Atualizar [cmake/OmaDependencyGraph.cmake](cmake/OmaDependencyGraph.cmake) p/ incluir `ALLOWED_DEPS_project-ir = base`.
3. Atualizar [libs/project/CMakeLists.txt](libs/project/CMakeLists.txt) p/ `DEPS timeline project-ir base` (alinha com target).
4. Criar esqueleto `libs/importers` com `ALLOWED_DEPS_importers = base project-ir`.

[Return to analysis-integral-consolidado.md](analysis-integral-consolidado.md)
