# Outros editores: o que as pessoas gostam

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Várias fontes são agregadores de reviews (G2, Capterra,
> comparativos); servem para identificar padrões de opinião, não como medição.
>
> Clipchamp está em [`movie-maker.md`](movie-maker.md) §5. iMovie, Premiere,
> Resolve e CapCut têm documentos próprios.

---

## 1. Final Cut Pro (Apple)

### O que as pessoas gostam
- **Timeline magnética**: fecha buracos automaticamente ao remover clips e
  evita desalinhamento acidental entre tracks; menos erros comuns de edição.
- **Desempenho**: feito para Apple Silicon e Metal; playback em tempo real de
  timelines complexas; tempo de render e resposta da timeline em projetos multi-stream 4K/HDR
  são motivos citados para escolhê-lo em vez de alternativas multiplataforma.
- **Background rendering**: o render acontece enquanto o usuário edita.
- **Compra única** (cerca de US$ 300) com atualizações gratuitas, sem assinatura.

### Implicações para o OmaMovie
- O desempenho do FCP vem de **otimização para uma plataforma específica**. É a
  mesma aposta do OmaMovie (Omarchy, Vulkan, VA-API, PipeWire), em vez de multiplataforma.
- **Background rendering** = cache de frames renderizados (`CLAUDE.md` §15)
  preenchido por jobs de baixa prioridade quando o playback em tempo real não é
  possível. Deve ser invisível: o usuário não clica em "renderizar".
- Timeline magnética: ver `imovie.md` §5 (política de edição, não restrição do modelo).
- Formato **FCPXML** é alvo de interop de alta prioridade (XML documentado).

---

## 2. Descript

### O que as pessoas gostam
- **Editar vídeo como um documento**: apagar uma frase da transcrição apaga o trecho de áudio/vídeo correspondente.
- Intuitivo e acessível a quem nunca editou vídeo.
- Ferramentas de IA para melhorar áudio.
- Muito usado por podcasters.

### Implicações para o OmaMovie
- Edição por texto também existe no Premiere (`premiere-pro.md` §7). É um
  padrão que veio para ficar em conteúdo falado.
- Arquiteturalmente, a transcrição é **mais uma visão sobre a timeline**: cada
  palavra tem um `TimeRange` na mídia, e apagar texto gera comandos normais de
  edição (ripple delete) com undo. Não é um modelo paralelo.
- Depende de transcrição (IA, não objetivo). Se um dia entrar: modelo local, e
  o modelo de dados (palavras com range de tempo) deve funcionar com qualquer fonte de transcrição, inclusive importada.

---

## 3. Kdenlive

Editor livre (KDE) baseado no framework **MLT**. É o concorrente mais direto do OmaMovie no Linux.

### O que as pessoas gostam
- Gratuito, código aberto, multitrack, chroma key, correção de cor, render 4K,
  keyframes, workspaces customizáveis.
- **Privacidade e uso offline**, sem anúncios nem cadastro.

### Estado atual (2025–2026)
- 2025 com foco declarado em **estabilidade**, desempenho da timeline, áudio e
  legendas; colaboração mais próxima com os desenvolvedores do MLT.
- 25.04: importar clip direto do menu de contexto da timeline; **zoom na
  direção do mouse** em vez do playhead; waveforms para sequências.
- 25.08.x: releases de manutenção com correções de crashes e regressões.
- **Em desenvolvimento**: suporte a cor **10/12 bits**, otimizações de playback
  (decode), **OpenFX**, refatoração de keyframes com **dopesheet**.

### Implicações para o OmaMovie
- O Kdenlive ainda está **adicionando** 10/12 bits e otimizações de decode. O
  OmaMovie pode tratar alta profundidade de cor e decode GPU como base desde o início (`CLAUDE.md` §7).
- O foco em estabilidade, depois de anos de reclamações, mostra que **estabilidade
  é o que os usuários de editores livres mais cobram**.
- Pequenos detalhes de UX que valem adotar: **zoom da timeline centrado no mouse**; importar direto da timeline.
- **Dopesheet** (ver keyframes de vários efeitos juntos) é uma boa referência para quando o OmaMovie tiver keyframes.
- Projetos do Kdenlive são XML do MLT: candidato natural a importer (formato aberto e texto).

---

## 4. Shotcut

Editor livre, também baseado em MLT.

### O que as pessoas gostam
- Gratuito e de código aberto, com multitrack, filtros e export confiável.
- 4K com fluidez graças à renderização por GPU.
- Interface simples e limpa, **comparada ao Windows Movie Maker**, mas com
  recursos avançados (correção de cor, chroma key) e workspace reorganizável.

### O que as pessoas não gostam
- Interface considerada **datada**; efeitos avançados são difíceis de fazer.
- Poucos efeitos.
- **Lag no preview** com arquivos grandes.
- Bugs específicos no Linux.

### Implicações para o OmaMovie
- A comparação com o Movie Maker mostra que **a referência mental de "editor
  simples" ainda é o Movie Maker**. Há demanda por esse posicionamento no Linux.
- As reclamações (preview lento, UI datada) são justamente o que o OmaMovie
  ataca: pipeline GPU-first e UI moderna e contextual.

---

## 5. Padrões entre todos os editores

O que aparece repetidamente como motivo de gostar (ou largar) um editor:

| Fator | Exemplos positivos | Exemplos negativos | Prioridade para o OmaMovie |
|---|---|---|---|
| **Estabilidade / não perder trabalho** | FCP | Premiere, CapCut, Shotcut | Máxima (§14, §18 do `CLAUDE.md`) |
| **Desempenho de playback** | FCP, Resolve (com hardware forte) | Shotcut com arquivos grandes, Resolve em máquina fraca | Máxima (GPU-first) |
| **Velocidade até o resultado** | CapCut, iMovie, Descript, página Cut do Resolve | Premiere/Resolve para iniciantes | Alta (presets, inspector contextual) |
| **Modelo de preço** | Resolve Free, FCP compra única, Kdenlive/Shotcut grátis | Premiere assinatura, CapCut paywall | Projeto livre: vantagem natural |
| **Privacidade / offline** | Kdenlive, Shotcut | CapCut (termos), Clipchamp (conta) | Natural (sem cloud) |
| **Legendas** | CapCut, Premiere, Resolve | — | Alta: track de legenda no modelo |
| **Vertical / redes sociais** | CapCut, Clipchamp | — | Média: troca de proporção do canvas |
| **Interface moderna e limpa** | CapCut, iMovie, Clipchamp | Shotcut ("datada"), Resolve ("densa") | Alta |

---

## Fontes

- [Fitgap: Final Cut Pro X](https://us.fitgap.com/products/final-cut-pro-x)
- [Temperstack: Final Cut Pro](https://www.temperstack.com/software/final-cut-pro/)
- [Gartner Peer Insights: Final Cut Pro](https://www.gartner.com/reviews/product/final-cut-pro)
- [Unite.AI: Descript review](https://www.unite.ai/descript-review)
- [G2: Descript reviews](https://www.g2.com/products/descript/reviews)
- [Tella: Descript video editing — how does it work](https://www.tella.com/blog/descript-video-editing-how-does-it-work)
- [Geeky Gadgets: Edit videos like a text doc](https://www.geeky-gadgets.com/edit-videos-like-a-text-doc/)
- [Kdenlive: 25.08.1 release](https://kdenlive.org/news/releases/25.08.1/)
- [Planet KDE: State of Kdenlive 2026](https://planet.kde.org/kdenlive-2026-04-18-state-of-kdenlive-2026/)
- [Feedbagel: Kdenlive 2025 development update](https://feedbagel.com/post/kdenlive-2025-development-update-focus-on-stability-performance-and-community-co)
- [Linux Today: Kdenlive 25.08.2 released](https://www.linuxtoday.com/blog/kdenlive-25-08-2-released-with-stability-fixes-and-polished-effects/)
- [Ubunlog: Kdenlive 25.12.2](https://en.ubunlog.com/kdenlive-25-12-2/)
- [AlternativeTo: Kdenlive alternatives](https://www.alternativeto.net/software/kdenlive/)
- [G2: Shotcut reviews](https://www.g2.com/products/Shotcut/reviews)
- [Capterra: Shotcut reviews](https://www.capterra.com/p/173467/Shotcut/reviews/?page=3)
- [VideoHelp: Shotcut reviews](https://videohelp.com/software/Shotcut/reviews)
- [rfp.wiki: Kdenlive vs Shotcut](https://www.rfp.wiki/design-multimedia/media-entertainment/video-editing-software/kdenlive/shotcut)
