## 1. Visão Geral

OmaMovie tem **arquitetura disciplinada e bem pensada**. Segue CLAUDE.md com rigor: fronteiras claras, tempo racional, GPU-first, sincronização explícita, modelo timeline puro, schema evolution seguro. O código reflete decisões maduras (ADR-0002/0004/0005/0006/0007/0016).

Principais destaques:
- Mesmo-device Vulkan + Qt RHI sem readback (correto para Wayland), com admission gate p/ threads concorrentes.
- VA-API→DMA-BUF importado pelo próprio app (solução pragmática ao leak FFmpeg 9.0.1).
- Effects stack por-clip bem desenhado ([ADR-0016](../adr/0016-video-effects.md)), TimeMap segmentado preparado p/ ramps.
- Project format com migrações, bounds, save atômico, preservação unknown fields top-level.

[Return to analysis-integral-consolidado.md](analysis-integral-consolidado.md)
