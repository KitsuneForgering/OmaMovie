# Microsoft Movie Maker (e sucessores): UI/UX

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Itens marcados **(observação geral)** vêm de conhecimento
> amplo sobre o produto, sem fonte específica nesta pesquisa.
>
> Cobre Windows Movie Maker (2000–2009), Windows Live Movie Maker (2009–2017)
> e, como contexto, os sucessores: o Video Editor do app Fotos e o Clipchamp.

---

## 1. Resumo

O Movie Maker foi, para muita gente, o primeiro editor de vídeo. As lições úteis são:

1. **Storyboard e timeline como duas visões do mesmo projeto.** Na última versão
   (2012) viraram uma só área com **zoom**: afastado, é storyboard; aproximado, é timeline.
2. **Fluxo guiado por tarefas** (capturar → editar → publicar).
3. **AutoMovie**: estilos pré-definidos que montam um filme automaticamente.
4. Lições negativas: **suporte a formatos restrito** na primeira versão e
   **remoção de recursos** entre versões, o mesmo erro do iMovie '08.

---

## 2. Histórico

| Data | Versão | Notas |
|---|---|---|
| 2000-09-14 | 1.0 (Windows Me) | Criticada pelo conjunto pequeno de recursos e por exportar só ASF |
| 2001 | 1.1 (Windows XP) | Export DV-AVI e WMV 8 |
| 2002-11 | **2.0** | Expansão grande de recursos; mais de 60 transições e 37 efeitos; efeitos customizáveis em XML |
| 2004–2005 | 2.1 (XP SP2), MCE 2005 | Gravação de DVD na Media Center Edition |
| 2007 | **6.0 (Vista)** | Efeitos em DirectX/Direct3D, HDV; **removeu a captura analógica** e a customização via XML. Versão 2.6 lançada para hardware antigo |
| 2009-08-19 | **Windows Live Movie Maker** | Reescrita: toolbar **ribbon** (estilo Office 2007), **AutoMovie**, publicação direta no YouTube/Facebook. **Removeu** estabilização e narração |
| 2010-08 | 2011 | Recuperou vídeo HD e captura de webcam |
| 2012-04 | 2012 | H.264/MP4 como export padrão; narração de volta |
| 2017-01-10 | — | **Descontinuado** |
| Windows 10 | Video Editor (app Fotos) | Substituto, com text-to-speech e OneDrive |
| 2021-09-08 | **Clipchamp** | Adquirido pela Microsoft; editor padrão do Windows 11; traz de volta a timeline multitrack |

Em 2013 a PCMag o chamou de "possivelmente a forma mais simples (e divertida) de combinar clips de vídeo".

---

## 3. Interface clássica (versões 2.x–6.0)

- **Painel de tarefas** à esquerda, organizado em etapas (capturar vídeo, editar
  filme, concluir filme) **(observação geral; Wikipedia descreve o layout de forma geral)**.
- **Collections**: organização da mídia importada.
- **Preview** em tempo real.
- **Storyboard / Timeline** na parte inferior, alternáveis:
  - **Storyboard**: caixas grandes de tamanho fixo para cada clip ou imagem e
    caixas menores entre elas para as transições. Serve para ordenar o material.
  - **Timeline**: o comprimento de cada retângulo é proporcional à duração;
    permite trim, ajuste da duração de transições e mostra a track de áudio.
  - Tracks separadas para vídeo, áudio/música e títulos.

### O que funcionava

- Storyboard é ótimo para a primeira montagem (ordem das cenas) e intimida menos.
- A transição como objeto visível **entre** dois clips, com caixa própria, deixa o conceito óbvio.
- O fluxo por etapas ensinava o processo de edição.

### O que não funcionava

- A alternância entre modos é uma troca de contexto: o usuário precisa saber em que modo está para entender o que pode fazer.
- O conjunto de tracks era fixo e limitado.

---

## 4. Windows Live Movie Maker (2009–2012)

- **Ribbon** no lugar de menus, com abas por categoria (Home, Animations, Visual Effects, Project, View) **(observação geral para os nomes das abas)**.
- **Área híbrida storyboard/timeline com slider de zoom** no canto inferior
  direito. Com o slider todo à esquerda, cada clip é um retângulo do mesmo
  tamanho (storyboard). Aproximando, os retângulos passam a refletir a duração (timeline).
- **AutoMovie**: escolhe-se um estilo e o app monta o filme com título, créditos, transições e música.
- **Publicação direta** em plataformas (YouTube, Facebook).
- Edição simplificada: uma track de vídeo principal, sem multitrack livre **(observação geral)**.

### Implicações para o OmaMovie

- **Zoom semântico** é a ideia mais aproveitável desta pesquisa. Um único
  componente de timeline, com um eixo de zoom que vai de "storyboard" (um
  thumbnail por clip, tamanho uniforme, foco na ordem) até "timeline" (proporcional
  ao tempo, foco no corte). Não há modo separado nem troca de contexto.
  - Requisito técnico: a TimelineView precisa de layout independente de escala
    linear no extremo afastado. O modelo da timeline não muda; é só a projeção visual.
  - Precisa ser medido: thumbnails para todos os clips no zoom mínimo exigem cache e virtualização da lista.
- O ribbon é um padrão de Office, não de editor de vídeo. Não adotar; o inspector contextual cumpre o mesmo papel com menos espaço fixo.
- AutoMovie e templates: mesma conclusão do iMovie (`imovie.md` §7): gerar timeline normal e editável.

---

## 5. Sucessores

### Video Editor do Fotos (Windows 10)

Substituiu o Movie Maker com uma abordagem ainda mais simples, baseada em
storyboard, com text-to-speech e integração com OneDrive. A Wikipedia observa
que a edição baseada em timeline só voltou com o Clipchamp, ou seja, o Windows
ficou anos sem um editor com timeline embutido.

### Clipchamp (Windows 11)

Layout em três colunas mais timeline:

- **Barra lateral esquerda com abas**: Your media, Record & create (webcam,
  tela, text-to-speech), Templates, Music & SFX, bibliotecas de vídeo/imagem
  (stock), Graphics, Text, Transitions, Brand kit.
- **Preview** no centro.
- **Timeline multitrack** embaixo.
- **Painel de propriedades à direita, contextual**: muda conforme o elemento
  selecionado, com abas de cor, áudio, filtros, velocidade, transições e texto.
- Templates por formato de destino (Instagram, TikTok etc.) e criação automática de vídeo com IA.
- Exportação em várias resoluções (4K no plano pago) e upload direto para redes sociais.
- Avaliações descrevem a interface como "refreshingly simple" e parecida com o Premiere na disposição.

### Implicações para o OmaMovie

- O Clipchamp chegou ao mesmo layout que o OmaMovie planeja: biblioteca à
  esquerda, preview no centro, inspector contextual à direita, timeline
  embaixo. É uma convergência que valida o desenho do `CLAUDE.md` §11.
- **Biblioteca com abas por tipo de conteúdo** (mídia, texto, transições,
  áudio) é uma alternativa ao "browser único" do iMovie. Para o OmaMovie, a
  versão sem conteúdo de stock: Mídia / Títulos / Transições / Efeitos.
- **Gravação de tela/webcam integrada** é um recurso com demanda real em
  editores simples. Em Omarchy/Wayland isso passaria por PipeWire e
  xdg-desktop-portal (ScreenCast). Não é prioridade; registrar como ideia futura.
- Contraste técnico: o Clipchamp é baseado em tecnologia web **(observação geral)**.
  O OmaMovie, nativo e GPU-first, deve competir em desempenho e em funcionar offline sem conta.

---

## 6. Lições negativas

| Erro | Versão | Lição para o OmaMovie |
|---|---|---|
| Exportava só ASF | 1.0 | Suporte amplo a formatos é parte da usabilidade. FFmpeg resolve isso; não restringir artificialmente |
| Removeu captura analógica | 6.0 | Mudança de plataforma não justifica perder capacidade |
| Removeu estabilização e narração | Live 2009 | Reescrita que remove recurso gera rejeição (mesmo caso do iMovie '08) |
| Removeu customização de efeitos | 6.0 e Live | Extensibilidade retirada não volta. Não prometer extensibilidade (plugins) antes de poder mantê-la, como já diz o `CLAUDE.md` |
| Descontinuado sem sucessor equivalente | 2017 | O usuário ficou anos sem timeline no Windows. Arquitetura que exige reescrita total tende a matar o produto; daí a regra de "crescer sem reescrever o núcleo" |

Um memorando interno de Bill Gates (janeiro de 2003), revelado em processo
antitruste, reclamava da dificuldade de baixar e instalar o Movie Maker.
**Lição:** a primeira experiência (instalar, abrir, importar) faz parte do produto.
Para o OmaMovie: empacotamento simples para Arch/Omarchy e primeiro uso sem configuração.

---

## Fontes

- [Wikipedia: Windows Movie Maker](https://en.wikipedia.org/wiki/Windows_Movie_Maker)
- [InformIT: Using Microsoft Movie Maker 2](https://informit.com/articles/article.aspx?p=370630)
- [USF Tech Ease: What is Windows Movie Maker](https://etc.usf.edu/techease/win/images/what-is-windows-movie-maker/)
- [O'Reilly: Customizing your Timeline view (Windows Movie Maker 2)](https://www.oreilly.com/library/view/microsoft-windows-movie/0321199545/0321199545_ch09lev1sec5.html)
- [XDA: How to use the Clipchamp video editor on Windows 11](https://www.xda-developers.com/how-to-use-clipchamp-video-editor-windows-11/)
- [ITPro: Clipchamp to be installed on Windows 11 by default](https://itpro.com/operating-systems/microsoft-windows/366289/clipchamp-video-editor-to-be-pre-installed-on-windows-11)
- [Help Desk Geek: How to use the Windows 11 video editor (Clipchamp)](https://helpdeskgeek.com/how-to-use-the-windows-11-video-editor-clipchamp/)
- [Windows Report: Clipchamp review](https://windowsreport.com/clipchamp-review/)
