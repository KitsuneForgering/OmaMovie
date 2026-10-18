## 3. Riscos Consolidados

| Risco | Categoria | Impacto | Probabilidade | Mitigação |
|---|---|---|---|---|
| **Flickering HW path (Hyprland+Quickshell)** | Integração | Alto | **Média-Alta** (relatado) | Verificar `acquire_foreign/release_foreign` no compositor ao usar frames DMA-BUF (M1 doc-C). Testar SW vs HW discriminando. Rodar validation layer com sync. |
| **Gap project-ir/importers** | Extensibilidade | Alto (design vs real) | Alta | Criar `libs/project-ir` + esqueleto `libs/importers`, alinhar grafo CMake (project depende project-ir). Começar com formato acessível (OTIO/FCPXML). |
| **M7 não conectado + export ausente** | Implementação | Alto | Alta | Conectar `libs/project` à app (Session/Editor: carregar/salvar projeto nativo). Implementar export (GPU→encode→mux) com admission gate adequado. |
| **Device loss recovery incompleto** | Implementação | Médio-Alto | Baixa-Média | Hoje trata como final (correto p/ device compartilhado c/ Qt). Se necessário recovery, requer novo device + nova window — complexo. Manter como está por ora, mas documentar. |
| **Efeitos timeline↔render desalinhados** | Extensibilidade | Médio | Baixa | Centralizar mapeamento (adaptador em playback ou util). Evita bugs ao adicionar novos efeitos. |
| **Leak FFmpeg suprimido em ASan** | Qualidade | Baixo | Baixa (monitorar) | Workaround import próprio evita leak no novo path; suprimido pode ser legado. Acompanhar FFmpeg 9.0.1. |

[Return to analysis-integral-consolidado.md](analysis-integral-consolidado.md)
