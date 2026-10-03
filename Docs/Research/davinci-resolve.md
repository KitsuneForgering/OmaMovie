# DaVinci Resolve: por que é um editor poderoso

> Pesquisa feita em 2026-10-02 (versão atual: Resolve 21.x). Fatos com fonte
> estão linkados na seção [Fontes](#fontes). Itens marcados **(conhecimento
> geral)** são amplamente documentados, mas sem fonte específica nesta pesquisa;
> verificar antes de usar como requisito.

---

## 1. Resumo

O Resolve é poderoso por cinco razões:

1. **Cor de nível de cinema.** Nasceu como sistema de correção de cor, e o
   grading por nós é considerado o padrão da indústria.
2. **Tudo num programa só.** Edição, composição (Fusion), cor, áudio
   (Fairlight) e entrega em "páginas" do mesmo app, sobre o mesmo projeto.
3. **Duas filosofias de edição.** A página **Cut** é rápida e enxuta; a página **Edit** é a NLE tradicional completa.
4. **Modelo de preço.** Versão gratuita muito capaz, e Studio com **compra única** (cerca de US$ 299).
5. **Roda nativamente em Linux**, o que nenhum dos grandes concorrentes faz.

Por outro lado: curva de aprendizado íngreme, exige hardware forte, e **no
Linux a versão gratuita não decodifica H.264/H.265 nem AAC**. Este último ponto
é muito relevante para o OmaMovie.

---

## 2. Histórico

**(conhecimento geral, salvo indicação)**

| Ano | Marco |
|---|---|
| anos 1980–2000 | da Vinci Systems fabrica sistemas de correção de cor para telecine |
| 2009 | Blackmagic Design compra a da Vinci |
| ~2011 | Versão gratuita (Lite) lançada |
| 2014 (v11) | Vira editor completo (página Edit) |
| 2017 (v14) | Fairlight (áudio) integrado |
| 2018 (v15) | Fusion (composição por nós) integrado |
| 2019 (v16) | Página **Cut** |
| 2020-11 | **Speed Editor** (teclado de edição dedicado) |
| v18 | "Databases" renomeados para "Project Libraries"; Blackmagic Cloud |
| v18.5 | Suporte nativo a **OpenTimelineIO** |
| 2025-05 (v20) | Mais de 100 recursos novos, incluindo ferramentas de IA |
| 2026-06 (v21) | Página **Photo**, mais ferramentas de IA |
| v21.1 | Suporte a **MCP** e 20 novas APIs de scripting |

---

## 3. Páginas: um app, vários contextos

| Página | Função |
|---|---|
| Media | Importação e organização |
| **Cut** | Montagem rápida |
| **Edit** | Edição tradicional completa |
| **Fusion** | Composição e VFX por nós |
| **Color** | Grading por nós |
| **Fairlight** | Mixagem e masterização de áudio |
| Deliver | Exportação |
| Photo (v21) | Edição de fotos com as ferramentas de cor do Resolve |

Todas as páginas operam sobre a mesma timeline e o mesmo projeto. Não há
export/import entre apps, ao contrário do Premiere + After Effects + Audition,
e isso é citado como motivo de migração (ver `premiere-pro.md` §10).

### Implicações para o OmaMovie

- Páginas são uma terceira forma de organizar a complexidade, ao lado dos
  workspaces do Premiere e do inspector contextual do iMovie. Elas funcionam
  para fluxos profissionais em fases (montar → corrigir cor → mixar), mas
  significam troca de contexto e uma interface densa em cada página.
- Para o OmaMovie, o inspector contextual continua sendo o modelo principal.
  Um "modo de foco" futuro (ex.: cor em tela cheia) pode existir, mas como
  ampliação do inspector, não como app dentro do app.
- A lição arquitetural mais forte: **um único modelo de projeto para todas as
  funções**. Cor, áudio e composição leem e escrevem o mesmo modelo. O OmaMovie
  já segue isso (`timeline` → render graph); não criar modelos paralelos por função.

---

## 4. Página Cut: velocidade para profissionais

Introduzida na v16 para montagens rápidas com prazo curto:

- **Source tape**: um botão mostra todos os clips do bin no viewer como **uma
  única fita contínua**. Dá para fazer scrub por todo o material e editar sem procurar clip por clip.
- **Timeline dupla**: a timeline de cima mostra **o programa inteiro**; a de
  baixo mostra **a região de trabalho** em zoom. As duas são editáveis. A
  Blackmagic justifica dizendo que dar zoom e rolar a timeline é lento.
- Interface enxuta, com menos ferramentas visíveis.
- Pensada para o **Speed Editor** (§9).

### Implicações para o OmaMovie

- A própria Blackmagic, dona do editor mais "completo", criou uma página
  simplificada. Confirma que **simples e rápido não são opostos**: a página
  Cut é simples e feita para profissionais com prazo.
- **Timeline dupla** é uma alternativa ao zoom semântico do Movie Maker
  (`movie-maker.md` §4). Os dois resolvem o mesmo problema (visão geral +
  detalhe sem dar zoom o tempo todo). Avaliar no protótipo da TimelineView:
  uma barra de visão geral compacta (estilo minimapa) acima da timeline pode dar o benefício sem duplicar a interface.
- **Source tape** é uma forma de navegar a biblioteca com o mesmo motor de
  skimming (`imovie.md` §6). Requisito técnico igual: seek rápido e cache de thumbnails.

---

## 5. Cor

- **Grading por nós**: cada nó é uma etapa da correção; os nós são conectados,
  reordenados e alterados sem afetar o resto do grade. Não destrutivo por construção.
- Ferramentas: rodas de cor, curvas, **qualifiers** (seleção por cor/luminância), **power windows** (máscaras com tracking).
- v21: visualização dos nós como **lista de camadas** (layer-list node graph),
  pilhas de até 8 camadas por nó, workflows ACES melhorados, grading em grupo com versões.
- Trims de HDR independentes para Dolby Vision, HDR10+ e HDR Vivid (21.1).

### Implicações para o OmaMovie

- **O Resolve v21 adicionou uma visualização em lista de camadas para o grafo
  de nós.** Mesmo para coloristas, um grafo puro nem sempre é a melhor
  interface. Para o OmaMovie: **render graph como DAG internamente, lista de camadas/efeitos na UI**.
  O grafo fica no núcleo; a UI mostra a pilha.
- Grading por nós completo é não objetivo inicial. Mas a interface de efeitos
  deve permitir que um efeito de cor tenha várias etapas internas, para não fechar a porta.
- ACES e HDR reforçam o ADR-0006 (espaço de cor de trabalho), na mesma linha do Premiere 25.2.

---

## 6. Fusion e Fairlight

- **Fusion**: composição por nós, integrada como página. A v20 trouxe
  composição multi-camada mais avançada; a v21/21.1 adicionou o toolset Krokodove (70+ gráficos, ferramentas 3D e procedurais).
- **Fairlight**: estação de áudio completa dentro do editor; v20 trouxe
  IntelliCut (remove silêncio e separa diálogos entre falantes); v21 trouxe folder tracks.

### Implicações para o OmaMovie

- Composição estilo After Effects/Fusion é **não objetivo**. A existência do
  Fusion mostra o teto da categoria, não um requisito.
- Áudio: remoção de silêncio é uma função concreta, útil para conteúdo falado
  (podcast, tutorial). Pode ser feita com análise de nível de áudio, sem IA.
  Candidata a recurso futuro de baixo custo.

---

## 7. Projetos: banco de dados, não arquivo

- Projetos vivem numa **Project Library** (antes "Database"):
  - local: estrutura gerenciada pelo Resolve no disco;
  - rede: **PostgreSQL** (via "DaVinci Resolve Project Server", que é um PostgreSQL empacotado);
  - **Blackmagic Cloud**: bibliotecas hospedadas para colaboração.
- Para obter um arquivo móvel é preciso **exportar um `.drp`**, que contém
  timelines, edições, grades e configurações, **sem mídia**. No destino, o Resolve faz relink.
- Fóruns registram crashes e falhas ao copiar bibliotecas para PostgreSQL.

### Implicações para o OmaMovie

- O modelo de banco é ótimo para colaboração em estúdio, mas cria atrito para
  o usuário individual: o projeto não é um arquivo que se copia, versiona ou envia.
- **Reforça a decisão do `CLAUDE.md` §14/§17**: o arquivo de projeto é a fonte de
  verdade; SQLite só para índices/cache/metadata. Um projeto do OmaMovie deve
  poder ser copiado, versionado em git e anexado num e-mail.
- `.drp` é alvo potencial de importação. Formato a investigar antes de priorizar (`Docs/formats/`).

---

## 8. Interoperabilidade

- **OpenTimelineIO nativo desde a v18.5**: import/export de `.otio` (só
  metadados da timeline) e `.otioz` (timeline mais mídia).
- XML, AAF e EDL **(conhecimento geral)**.

### Implicações para o OmaMovie

- **OTIO é o primeiro alvo de interop.** Resolve (e outros) exportam OTIO
  nativamente. Isso permite um caminho Resolve → OmaMovie **sem reverse engineering**,
  validando `Importer → ProjectIR` com um formato aberto e documentado.
- `.otioz` (timeline + mídia) é um bom modelo para um eventual "exportar projeto empacotado" do OmaMovie.

---

## 9. Hardware dedicado: Speed Editor

- Teclado de edição feito para a página Cut (lançado em novembro de 2020).
- **Search dial** de metal pesado para percorrer a timeline rápido; com um
  botão de trim pressionado, o dial vira um controle de **trim em tempo real**.
- Botões de shuttle/jog/scroll mudam o modo do dial; uma tecla por função de edição.
- USB-C ou Bluetooth.

### Implicações para o OmaMovie

- Mostra o valor da edição sem mouse: o mesmo princípio do J/K/L do `CLAUDE.md` §11.2.
- O sistema central de ações deve ser **independente do dispositivo de entrada**:
  teclado hoje, controladores jog/shuttle (USB HID/MIDI) no futuro, mapeados para as mesmas ações. Não implementar agora; só não acoplar ações a eventos de teclado.

---

## 10. Scripting, IA e MCP

- API de scripting (Python/Lua) **(conhecimento geral)**. A 21.1 adicionou 20 APIs
  (render presets, media pool, multicam, propriedades de timeline, normalização de áudio) e **removeu o Python 2**.
- **MCP (21.1)**: assistentes como Claude e ChatGPT Codex controlam o Resolve
  por linguagem natural (analisar projetos, organizar mídia, mudar configurações, render em lote, criar highlight reels).
- IA da v20: IntelliScript (timeline a partir de roteiro), legendas animadas,
  Multicam SmartSwitch (troca de ângulo por quem está falando), Magic Mask, depth map.
- IA da v21: IntelliSearch, CineFocus, refinamento facial.

### Implicações para o OmaMovie

- O sistema de comandos da timeline (`CLAUDE.md` §10: toda edição é um comando)
  é a base natural para scripting e automação. Um comando bem definido serve à
  UI, à CLI, a scripts e a um eventual servidor MCP.
- **Não implementar scripting agora.** Só manter os comandos serializáveis e
  sem dependência de UI, o que já é exigido para undo/redo e testes.

---

## 11. Resolve no Linux

Muito relevante, porque é o editor profissional que um usuário de Omarchy usaria hoje.

| Aspecto | Situação |
|---|---|
| Distro oficial | Ambiente baseado em **Rocky Linux 8.6**; Ubuntu/Mint funcionam na prática |
| GPU | **NVIDIA com drivers proprietários** é o único caminho oficialmente suportado (CUDA ≥ 12.8, ≥ 4 GB VRAM). AMD com AMDGPU Pro funciona, mas efeitos exclusivos de CUDA ficam lentos. Intel não é mencionado |
| H.264/H.265 (Free) | **Decode e encode não suportados** (licenciamento) |
| H.264/H.265 (Studio) | Decode suportado; **encode só com NVIDIA** |
| AAC | **Não suportado no Linux**, nem no Studio |
| Áudio | ALSA documentado; PipeWire não é mencionado |
| Contorno comum | Transcodificar com FFmpeg para DNxHR e remuxar áudio para PCM antes de importar |

### Implicações para o OmaMovie

**Esta é a maior oportunidade encontrada na pesquisa.** No Omarchy, o
usuário típico tem vídeo de celular (H.264/HEVC + AAC), muitas vezes GPU AMD ou
Intel e PipeWire. O Resolve gratuito não abre esse material sem transcodificar,
e o suporte fora da NVIDIA é parcial.

O OmaMovie pode ocupar exatamente esse espaço:
- H.264/HEVC/AV1 + AAC via FFmpeg e VA-API, **sem transcodificação**;
- **Intel e AMD como cidadãos de primeira classe** (VA-API/Vulkan Video), não só NVIDIA;
- PipeWire nativo;
- empacotamento para Arch, em vez de "suportado em Rocky Linux".

Isso reforça as prioridades do `CLAUDE.md` §7.3. Os testes de hardware devem
cobrir AMD e Intel, não só NVIDIA.

---

## 12. Fraquezas e críticas

- **Curva de aprendizado íngreme** e interface densa; padrões pensados para profissionais assustam iniciantes.
- **Exige hardware forte**; reclamações frequentes de desempenho e crashes em máquinas modestas.
- Limitações da versão gratuita em resolução, efeitos, codecs e uso de GPU.
- Linux: limitações de codec e de GPU (§11).

### Implicações para o OmaMovie

- O Resolve mostra o teto de capacidade, mas não atende quem quer editar rápido
  num laptop. O OmaMovie deve **funcionar bem em hardware modesto** (GPU integrada
  Intel/AMD), com proxies e degradação explícita em vez de travar.
- Benchmarks (`CLAUDE.md` §23) devem incluir pelo menos uma máquina com GPU integrada.

---

## 13. Tabela de priorização para o OmaMovie

| Capacidade do Resolve | Adotar? | Quando | Módulo |
|---|---|---|---|
| Modelo único de projeto para todas as funções | Sim (já previsto) | Desde o início | `timeline`, `project` |
| Visão geral + detalhe da timeline (timeline dupla / minimapa) | Avaliar no protótipo | Fase 10 | UI |
| Source tape (biblioteca como fita contínua) | Avaliar | Após biblioteca | UI, `media` |
| Render graph DAG interno, UI em pilha de camadas | Sim | Fase 5 | `compositor` |
| Import OTIO | Sim, primeiro alvo de interop | Fase 12 | `importers` |
| Projeto em banco de dados | **Não**; arquivo é a fonte de verdade | — | `project` |
| Remoção de silêncio | Futuro, sem IA | Após áudio | `audio` |
| Ações independentes do dispositivo de entrada | Sim (desenho) | Fase 10 | UI |
| Comandos serializáveis (base para scripting/MCP) | Sim (desenho) | Fase 9 | `timeline` |
| Scripting/MCP | Não agora | Futuro | — |
| Fusion/Fairlight completos | Não | Não objetivo | — |
| Codecs consumidor + AMD/Intel + PipeWire no Linux | **Sim, diferencial** | Fases 2–8 | `media`, `audio` |
| Import `.drp` | Investigar | Futuro | `importers` |

---

## Fontes

- [Blackmagic Design: DaVinci Resolve 20 announcement](https://www.blackmagicdesign.com/media/partial/release/20250404-02)
- [Post Magazine: Blackmagic Design unveils DaVinci Resolve 20](https://www.postmagazine.com/Press-Center/Daily-News/2025/Blackmagic-Design-unveils-DaVinci-Resolve-20-wit.aspx)
- [Post Magazine: Blackmagic Design releases v20 of DaVinci Resolve & Fusion](https://www.postmagazine.com/Press-Center/Daily-News/2025/Blackmagic-Design-releases-V-20-of-DaVinci-Resol.aspx)
- [Newsshooter: DaVinci Resolve 20 announced](https://www.newsshooter.com/2025/04/04/blackmagic-design-davinci-resolve-20-announced-with-100-new-features-including-ai-enhancements/)
- [Broadcast Now: NAB 2025 — AI rough cut and audio generation in Resolve](https://www.broadcastnow.co.uk/production-and-post/nab-2025-blackmagic-adds-ai-powered-rough-cut-and-audio-generation-to-resolve/5203915.article)
- [PetaPixel: DaVinci Resolve 21 officially released](https://petapixel.com/2026/06/03/davinci-resolve-21-officially-released-with-new-photo-editing-ai-tools/)
- [CineD: DaVinci Resolve 21 announced](https://www.cined.com/davinci-resolve-21-announced-new-photo-page-eight-new-ai-tools-tethered-camera-controls-and-more/)
- [Motion Media: DaVinci Resolve 21.1 update](https://www.motionmedia.com/mm-blog/davinci-resolve-21-1-update-new-features)
- [RedShark News: DaVinci Resolve 21.1 new features](https://www.redsharknews.com/davinci-resolve-21.1-new-features-release)
- [Storyblocks: Color grading in DaVinci Resolve](https://www.storyblocks.com/resources/tutorials/davinci-resolve-color-grading)
- [TourBox: A complete guide to nodes in DaVinci Resolve](https://www.tourboxtech.com/en/news/davinci-resolve-nodes.html)
- [Pro Video Coalition: DaVinci Resolve 16](https://www.provideocoalition.com/davinci-resolve-16-adds-lufs-audio-loudness-standards-linear-features/)
- [Wipster: Inside DaVinci Resolve's new Cut page](https://wipster.io/blog/inside-davinci-resolves-new-cut-page)
- [PremiumBeat: DaVinci Resolve 16 Cut page review](https://www.premiumbeat.com/blog/davinci-resolve-16-cut-page-review/)
- [Uncle: DaVinci Resolve Project Library vs database](https://tryuncle.com/learn/davinci-resolve/davinci-resolve-project-library-vs-database)
- [Blackmagic Forum: Copy DB from local to Postgres fails or crashes](https://forum.blackmagicdesign.com/viewtopic.php?p=746517)
- [Prism Pipeline: OpenTimelineIO integration](https://prism-pipeline.com/docs/latest/general/miscellaneous/opentimelineio/)
- [Digital Production: DaVinci Resolve Speed Editor release](https://digitalproduction.com/2020/11/11/davinci-resolve-speed-editor-release/)
- [tal.org: Blackmagic DaVinci Resolve on Linux](https://www.tal.org/tutorials/blackmagic-davinci-resolve-linux)
- [NixOS Wiki: DaVinci Resolve](https://wiki.nixos.org/wiki/DaVinci%20Resolve)
- [Blackmagic Forum: Can't play clips, use CUDA, .mp4 or h.264 in Linux](https://forum.blackmagicdesign.com/viewtopic.php?p=742208)
- [Movavi: How to use DaVinci Resolve (beginner's guide)](https://www.movavi.com/support/how-to/how-to-use-davinci-resolve/)
- [rfp.wiki: DaVinci Resolve vs Final Cut Pro](https://www.rfp.wiki/vendors/davinci-resolve/final-cut-pro)
