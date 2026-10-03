# Pesquisa: referências de produto

Pesquisa de mercado e de UX para orientar o OmaMovie. Não é especificação:
recomendações daqui só viram regra quando forem para o `CLAUDE.md` ou para um ADR.

**Produto e UX**

| Documento | Assunto |
|---|---|
| [premiere-pro.md](premiere-pro.md) | Por que o Adobe Premiere Pro é um editor poderoso (motor, modelo de edição, cor, interop, fraquezas) |
| [imovie.md](imovie.md) | UI/UX do Apple iMovie (layout, inspector contextual, timeline magnética, templates) |
| [movie-maker.md](movie-maker.md) | UI/UX do Windows Movie Maker e sucessores (storyboard/timeline, Clipchamp) |
| [davinci-resolve.md](davinci-resolve.md) | Por que o DaVinci Resolve é poderoso (páginas, Cut page, cor por nós, projetos em banco, OTIO, situação no Linux) |
| [capcut.md](capcut.md) | O que as pessoas gostam e não gostam no CapCut (templates, legendas, speed ramps, paywall, termos de uso) |
| [other-editors.md](other-editors.md) | Final Cut Pro, Descript, Kdenlive, Shotcut e padrões comuns entre editores |

**Tecnologia e plataforma**

| Documento | Assunto |
|---|---|
| [hardware-strategy.md](hardware-strategy.md) | **Síntese**: papel de cada API, device Vulkan único, seleção em runtime, mudanças propostas ao `CLAUDE.md`, spikes |
| [vulkan.md](vulkan.md) | Vulkan-Hpp, Vulkan Video, VA-API→Vulkan, FFmpeg `hwcontext_vulkan`, Qt Quick sobre device externo, libplacebo |
| [cuda.md](cuda.md) | NVDEC/NVENC sem toolkit, interop CUDA↔Vulkan, kernels via PTX (clang), notebooks híbridos |
| [opencl.md](opencl.md) | Análise que levou à **remoção do OpenCL** (rusticl, NEO, ROCm, interop com Vulkan, lição do Blender) |
| [omarchy-integration.md](omarchy-integration.md) | Tema, opacidade de janela, gravações de tela, shell Quickshell, menu, drivers instalados, distribuição |

## Método

- Fontes: documentação oficial (Adobe Help, Apple Support, Blackmagic, Kdenlive), Wikipedia,
  imprensa especializada, benchmarks independentes (Puget Systems) e agregadores de reviews
  (G2, Capterra; tratados como indício de opinião, não como medição). Links em cada documento.
- Afirmações sem fonte específica estão marcadas como **(conhecimento geral)** ou **(observação geral)**.
- Cada seção termina com **Implicações para o OmaMovie**, ligando o achado às seções do `CLAUDE.md`.
- Objetivo: extrair princípios. Copiar identidade visual ou comportamento proprietário é proibido (`CLAUDE.md` §11).

---

## Síntese

### Onde cada produto se posiciona

```
simples ◄──────────────────────────────────────────────────────────────► poderoso
Movie Maker/Fotos   iMovie    CapCut/Clipchamp   Shotcut/Kdenlive   FCP   Premiere/Resolve
(storyboard)    (motor FCP X) (presets, social)   (livres, MLT)          (cockpit/páginas)

OmaMovie: superfície próxima do iMovie/CapCut, motor próximo do Premiere/Resolve.
No Linux, o espaço entre Kdenlive/Shotcut e o Resolve está vazio.
```

### Conclusões

1. **Simplicidade vem de revelação contextual, não de remover recursos.**
   A barra de ajustes do iMovie e o painel de propriedades do Clipchamp mostram
   só o que se aplica à seleção. O Premiere resolve com 16 workspaces. O
   inspector contextual do OmaMovie é o caminho certo. Adotar também o
   **indicador visual de quais ajustes estão ativos** no clip (iMovie).

2. **Redesenho que remove capacidade gera rejeição.** iMovie '08 e Windows Live
   Movie Maker tiveram que devolver recursos nas versões seguintes. Confirma
   "esconder, não remover" e "crescer sem reescrever o núcleo".

3. **Motor profissional sob UI simples funciona.** O iMovie usa o motor do
   Final Cut Pro X desde 2013 e exporta para ele. É a tese do OmaMovie, com a
   diferença de ser um app só: exige mais cuidado com a revelação progressiva.

4. **Operações nomeadas sobre um compositor genérico.** PiP, cutaway, split screen
   e green screen do iMovie são presets. No OmaMovie, devem gerar layers e
   parâmetros normais no render graph, editáveis depois, sem o teto de camadas do iMovie.

5. **Zoom semântico storyboard ↔ timeline** (Movie Maker 2012). Uma TimelineView
   só, em que o zoom mínimo vira storyboard (clips de tamanho igual, foco na
   ordem). Candidato forte a diferencial de UX. Requer virtualização e cache de thumbnails.

6. **"Poderoso" começa no motor**: playback em tempo real de mídia nativa via GPU,
   decode/encode por hardware com fallback, pipeline em float, proxies. O
   Premiere mostra também que **efeitos sem GPU precisam de um caminho visível** (barra de render).

7. **Modelo de edição completo no núcleo, UI gradual.** Ripple/roll/slip/slide,
   split edits e três pontos devem existir como comandos da timeline cedo. O
   iMovie mostra que roll e split edit podem ter UI amigável (Precision Editor,
   Clip Trimmer). O Premiere mostra o acesso completo por teclado.

8. **Gerenciamento de cor automático** (Premiere 25.2: detecção de espaço de cor,
   presets, efeitos que conhecem o espaço de cor). Alimenta o ADR-0006.

9. **Estabilidade é parte do poder.** A principal crítica ao Premiere é
   instabilidade e projetos corrompidos. Salvamento atômico, migrações
   testadas e fuzzing de parsers são vantagem competitiva, não burocracia.

10. **`.prproj` é XML comprimido com gzip.** Alvo de importação relativamente
    acessível para validar `Importer → ProjectIR`, depois de OTIO/FCPXML/EDL.

11. **O Resolve no Linux deixa um espaço aberto.** A versão gratuita não decodifica
    H.264/H.265 nem AAC no Linux, o suporte oficial é só NVIDIA e o áudio documentado é ALSA.
    Vídeo de celular + GPU AMD/Intel + PipeWire, que é o cenário típico do Omarchy,
    é mal atendido. Esse é o **posicionamento mais claro** para o OmaMovie.

12. **Simples e rápido não são opostos.** A Blackmagic criou a página Cut
    (source tape, timeline dupla) para profissionais com prazo. O CapCut vence por
    velocidade até o resultado. A simplicidade do OmaMovie deve servir também ao usuário avançado.

13. **Visão geral + detalhe na timeline**: timeline dupla (Resolve) e zoom semântico
    (Movie Maker) atacam o mesmo problema. Prototipar as duas abordagens (ou um minimapa) na TimelineView.

14. **Grafo por dentro, pilha por fora.** O Resolve 21 adicionou uma visualização em
    lista de camadas ao grafo de nós de cor. O render graph do OmaMovie é um DAG; a UI mostra a pilha.

15. **Projeto em arquivo, não em banco.** Os projetos do Resolve vivem num banco
    (local/PostgreSQL/nuvem) e precisam de export `.drp` para virar arquivo.
    Confirma o `CLAUDE.md` §14/§17: arquivo é a fonte de verdade.

16. **OTIO é o primeiro alvo de interop.** O Resolve exporta OTIO nativamente
    desde a 18.5: caminho Resolve → OmaMovie sem reverse engineering.

17. **Operações de resultado, não de mecanismo.** O CapCut vende "hero time",
    não "keyframes de velocidade". Presets por intenção primeiro, parâmetros depois.

18. **Os pontos fracos dos concorrentes são os diferenciais do OmaMovie**:
    paywall e termos sobre o conteúdo (CapCut), assinatura (Premiere), crashes com
    perda de trabalho (Premiere, CapCut, Shotcut), preview lento (Shotcut),
    curva de aprendizado e hardware exigente (Resolve). Offline, sem conta, estável e GPU-first.

### Requisitos de arquitetura que surgiram

| Requisito | Origem | Afeta |
|---|---|---|
| Seek rápido e cache de thumbnails em várias densidades, com cancelamento agressivo | Skimming (iMovie), zoom semântico (Movie Maker) | `media`, cache, job system |
| Overlays interativos no preview que emitem comandos | Crop/posição/conta-gotas no viewer (iMovie) | PreviewPanel, `timeline` |
| Magnetismo como política de edição da UI, não restrição do modelo | Timeline magnética (iMovie/FCP X) | `timeline` |
| Capacidade GPU declarada por efeito e indicação de trechos fora do tempo real | Mercury (Premiere) | `compositor`, UI |
| Templates geram timeline comum e editável | Trailers (iMovie), AutoMovie | `timeline`, `project-ir` |
| Metadados de cor por stream e efeitos que conhecem o espaço de cor | Color management (Premiere) | `media`, `compositor` |
| Edição durante o playback (trim dinâmico) | Premiere | Threading (ADR-0003) |
| **Mapeamento de tempo por curva** (não só velocidade constante), decidido antes de implementar velocidade | Speed ramps (CapCut) | `base` time, `timeline` (ADR-0002) |
| Track de legenda/texto de primeira classe, import/export SRT/VTT | CapCut, Premiere, Resolve | `timeline`, `project` |
| Canvas com proporção alterável; decidir coordenadas normalizadas vs. pixels | Vertical/horizontal (CapCut, Clipchamp) | `compositor`, `timeline` |
| Marcadores no modelo da timeline | Beat sync (CapCut), revisão | `timeline` |
| Comandos serializáveis e sem dependência de UI (base para scripting/MCP futuros) | Scripting e MCP (Resolve 21.1) | `timeline` |
| Sistema de ações independente do dispositivo de entrada | Speed Editor (Resolve) | UI |
| Render em background preenchendo o cache quando o tempo real não é possível | Final Cut Pro | `compositor`, cache, job system |
| Transcrição como visão da timeline (palavras com `TimeRange`) | Descript, Premiere | `timeline` (futuro) |
| Benchmarks incluindo GPU integrada Intel/AMD | Resolve exige hardware forte | `tools/bench` |

### Próximas pesquisas sugeridas

- **OpenTimelineIO**: modelo de dados como referência para o ProjectIR (prioridade, dado o suporte nativo do Resolve).
- **Final Cut Pro em profundidade**: storylines, roles, FCPXML (alvo de interop).
- **Kdenlive/MLT por dentro**: arquitetura do MLT, por que 10-bit e decode GPU chegaram tarde, formato de projeto (candidato a importer).
- **Pipeline de hardware no Linux**: estado atual de VA-API, Vulkan Video e interop DMA-BUF em Mesa/NVIDIA (base técnica para §7.3 do `CLAUDE.md`).
- **Avid Media Composer e Vegas**: modelo de edição e formatos (alvos de interop futuros).
