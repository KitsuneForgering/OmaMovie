# Apple iMovie: UI/UX

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Itens marcados **(observação geral)** vêm de conhecimento
> amplo sobre o produto, sem fonte específica nesta pesquisa; verificar antes
> de usar como requisito.
>
> O objetivo é extrair **princípios** de UX. O `CLAUDE.md` proíbe copiar a
> identidade visual ou comportamento proprietário do iMovie.

---

## 1. Resumo

O iMovie é simples porque:

1. a tela tem **três áreas fixas** (browser, viewer, timeline) e quase nada mais;
2. os controles de ajuste aparecem **por contexto**, numa barra acima do viewer;
3. efeitos de composição são oferecidos como **operações nomeadas por intenção**
   (picture-in-picture, cutaway, split screen, green screen), não como camadas e parâmetros genéricos;
4. a timeline é **magnética**: não deixa buracos e evita colisões;
5. há **templates** (temas, trailers, storyboards, Magic Movie) para quem não quer montar do zero;
6. por baixo, desde 2013, usa o **motor do Final Cut Pro X**, e o projeto pode ser enviado para o Final Cut Pro.

O item 6 é o mais relevante para o OmaMovie: é exatamente "UI simples, pipeline sério".

---

## 2. Histórico relevante para UX

| Ano | Versão | Mudança |
|---|---|---|
| 1999 | iMovie 1 | Lançado com o iMac DV |
| 2004 | iMovie 4 | Edição não destrutiva ("Direct Trimming") |
| 2005 | iMovie HD (5/6) | Suporte HDV |
| 2007 | **iMovie '08** | Reescrita total por Randy Ubillos, focada em velocidade. Introduziu **skimming** e **Events** (clips agrupados por dia). **Muito criticada**: removeu áudio na timeline e plugins. David Pogue chamou de "utter bafflement" |
| 2009–2010 | iMovie '09 / '11 | Recuperou recursos removidos; adicionou **trailers** e **Precision Editor** |
| 2013 | **iMovie 10** | Nova reescrita sobre o **motor do Final Cut Pro X**; bibliotecas |
| 2015 | 10.1 | Edição 4K |
| 2020 | 10.2 | Apple silicon |
| 2022 | iOS 3.0 | **Storyboards** e **Magic Movie** |

### Lição do iMovie '08

A reescrita trouxe ideias boas (skimming, organização por eventos), mas
removeu capacidades que os usuários já tinham. A reação negativa obrigou a
Apple a recolocar recursos nas duas versões seguintes.

**Para o OmaMovie:** simplificar a UI **nunca pode significar remover
capacidade** já entregue. É o princípio "esconder, não remover" do `CLAUDE.md`
(§11), com um caso histórico concreto.

---

## 3. Layout da janela

Segundo o guia oficial, a janela tem três áreas:

- **Browser**: biblioteca de mídia e conteúdo (transições, títulos, áudio, fundos).
- **Viewer**: preview, com filtros e efeitos aplicados.
- **Timeline**: sequência do projeto, vídeo em cima e áudio embaixo.

Detalhes de interação:

- **Um único divisor redimensiona tudo**: arrastar a borda entre viewer e
  timeline para cima ou para baixo ajusta as três áreas proporcionalmente.
- O **Libraries List** e o **browser** podem ser escondidos (botão "Content Library" na toolbar).
- **Window > Revert to Original Layout** restaura o layout padrão.
- Não há painéis livres nem workspaces.

### Implicações para o OmaMovie

- O layout-alvo do `CLAUDE.md` (MediaPanel, PreviewPanel, TimelineView,
  Inspector, TransportControls) é parecido em espírito. Para manter a
  simplicidade: **poucos divisores, layout fixo, "restaurar layout" sempre disponível**.
- Em Hyprland o usuário já gerencia janelas em tiling. Uma janela única com
  layout interno fixo combina melhor com isso do que painéis flutuantes.

---

## 4. Barra de ajustes contextual

- A **barra de ajustes** fica acima do viewer. Ao selecionar um clip no browser
  ou na timeline, os botões dela mostram os controles correspondentes.
- Exemplo documentado (cor): o botão **Color Balance** e o botão **Color
  Correction**, que mostra um controle multislider, saturação e temperatura de cor.
- O estado aplicado é visível: os botões ficam destacados quando há um ajuste automático aplicado ao clip.
- Ferramentas interativas (ex.: conta-gotas) aparecem **no viewer** quando necessárias.
- O guia lista outros ajustes do mesmo tipo: auto enhance, crop, rotação, Ken
  Burns, estabilização, filtros, velocidade, volume.

### Implicações para o OmaMovie

- É o modelo do **Inspector contextual** do `CLAUDE.md` §11, com um detalhe a
  adotar: **mostrar visualmente quais ajustes estão ativos no clip** (badge ou destaque), sem abrir cada seção.
- **Manipulação direta no preview** (crop, posição, conta-gotas) é melhor do
  que campos numéricos para a maioria dos usuários. Os valores numéricos devem
  existir no inspector para precisão. Implicação técnica: o PreviewPanel precisa
  de overlays interativos que conversem com o modelo via comandos (com undo), sem guardar estado próprio.

---

## 5. Timeline e edição

### Timeline magnética

O conceito vem do Final Cut Pro X, cujo motor o iMovie usa: os clips se ajustam
"magneticamente" ao redor de um clip arrastado, e quando um clip é removido os
vizinhos fecham o espaço. Não há buracos acidentais.

### Métodos de trim (documentados)

1. **Arrastar bordas** do clip na timeline.
2. **Clip Trimmer**: mostra a parte não usada do clip esmaecida; permite
   estender, encurtar ou deslizar o trecho usado sem mudar a duração (equivale a um **slip**).
3. **Precision Editor**: duplo clique na borda; ajusta o início e o fim dos
   clips e a duração das transições; permite **split edits** (áudio e vídeo com pontos de corte diferentes, J-cut/L-cut).
4. **Seleção de range** (tecla R + arrastar) e "Trim Selection".

### Overlays como operações nomeadas

Picture-in-picture, cutaway, split screen e green screen são oferecidos como
**efeitos de alto nível**: o usuário coloca um clip acima de outro e escolhe a
intenção. Não precisa entender camadas, máscaras ou blend modes. A capacidade de
composição é **limitada** (não há múltiplas camadas arbitrárias de vídeo nem
keyframes gerais) **(observação geral)**.

### Implicações para o OmaMovie

- **Timeline magnética como comportamento padrão da UI**, implementada como
  ripple no modelo. O modelo da timeline continua genérico (tracks livres,
  buracos permitidos); o "magnetismo" é uma política de edição aplicada pelos comandos da UI.
- **Precision Editor = UI amigável para roll e split edits.** Mostra que trim
  avançado pode ser simples se a visualização for boa (mostrar o material
  não usado). O modelo de comandos de §3 de `premiere-pro.md` serve aos dois públicos.
- **Operações nomeadas compiladas para o compositor genérico**: "Picture in
  picture" na UI vira layers + transform + crop + borda no render graph. O
  OmaMovie pode oferecer os presets do iMovie **sem** a limitação de camadas,
  porque o compositor é genérico. Isso aplica diretamente o princípio "UI simples, pipeline sério".
- O preset deve continuar editável: depois de aplicar "PiP", o inspector mostra
  transform/crop normais, e o usuário pode ajustar além do que o preset previa.

---

## 6. Biblioteca e skimming

- **Skimming** (desde o iMovie '08): passar o mouse sobre um clip mostra o
  conteúdo em velocidade controlada pelo usuário, sem precisar dar play.
- O tamanho das miniaturas é ajustável por slider, assim como quanto tempo cada
  miniatura representa e a exibição de waveforms.
- Clips podem ser **avaliados** (favorito/rejeitado) e filtrados.
- Mídia organizada em **bibliotecas** e **eventos**.

### Implicações para o OmaMovie

- Skimming é a interação que mais diferencia uma biblioteca "visual". **Exige
  arquitetura**: decode rápido de frames arbitrários (seek), cache de
  miniaturas em várias densidades, e trabalho fora da thread de UI com
  cancelamento agressivo (o mouse se move mais rápido que o decode). Isso
  entra nos requisitos do cache (§15) e do job system (§13) do `CLAUDE.md`.
- Avaliação de clips (favorito/rejeitado) é metadata leve. É um bom caso para SQLite (§17).

---

## 7. Templates e automação

- **Temas**: estilo visual aplicado a títulos e transições do filme inteiro.
- **Trailers**: templates da Apple por gênero, com shot list e títulos; podem ser convertidos em projeto normal.
- **Storyboards** (iOS 3.0): ~20 roteiros sugeridos (receita, Q&A, review de
  produto, reportagem), com dicas por cena; reordenáveis e editáveis.
- **Magic Movie** (iOS 3.0): gera um vídeo a partir de clips selecionados,
  identificando diálogo, rostos e ações, e adiciona transições, música e títulos.

### Implicações para o OmaMovie

- O ponto mais importante: **trailer pode ser convertido em projeto normal**.
  Templates devem gerar estruturas comuns da timeline, não um modo especial
  fechado. No OmaMovie, um template seria um gerador de comandos/ProjectIR, sem tipo de projeto separado.
- Templates e geração automática não são prioridade (não objetivos iniciais),
  mas o desenho "template → timeline normal" custa pouco se for previsto.

---

## 8. Caminho para o profissional

- O iMovie usa o motor do Final Cut Pro X desde 2013.
- O guia documenta **enviar projetos para o Final Cut Pro**.

### Implicações para o OmaMovie

- Valida a tese do projeto: uma UI simples sobre um motor profissional, sem
  beco sem saída quando o usuário cresce.
- Diferença para o OmaMovie: a Apple tem dois apps; o OmaMovie quer **um app**
  em que a capacidade avançada fica escondida, não em outro produto. Isso exige
  mais disciplina no inspector contextual e na revelação progressiva.

---

## 9. Atalhos

O guia tem uma página de atalhos de teclado (navegação de janelas, edição,
playback). O iMovie é utilizável pelo teclado, mas é orientado ao mouse **(observação geral)**.

**Para o OmaMovie:** o `CLAUDE.md` exige edição completa por teclado, ou seja,
ir além do iMovie nesse ponto, mais próximo do Premiere.

---

## 10. O que não copiar

- Identidade visual, ícones, nomes de marca ("Magic Movie", "Precision Editor") e trade dress.
- **Limitações de capacidade** que existem só para manter a simplicidade
  (número fixo de camadas, sem keyframes). O OmaMovie esconde complexidade, não remove.
- Dependência de ecossistema (Fotos, iCloud).

---

## Fontes

- [Apple: iMovie User Guide for Mac](https://support.apple.com/guide/imovie/welcome/mac)
- [Apple: Change the window layout in iMovie on Mac](https://support.apple.com/guide/imovie/change-the-window-layout-movcab8cb6ef/mac)
- [Apple: Trim clips in iMovie on Mac](https://support.apple.com/guide/imovie/trim-clips-movf8b8fc9b2/mac)
- [Apple: Adjust a clip's color in iMovie on Mac](https://support.apple.com/guide/imovie/adjust-a-clips-color-movf2a3f0f42/mac)
- [Apple: Keyboard shortcuts in iMovie on Mac](https://support.apple.com/guide/imovie/keyboard-shortcuts-movd9d8f91e8/mac)
- [Apple: Send projects to Final Cut Pro](https://support.apple.com/guide/imovie/send-projects-to-final-cut-pro-movcbf7e2a3f/mac)
- [Apple: Final Cut Pro interface (Magnetic Timeline, browser, viewer)](https://support.apple.com/en-vn/guide/final-cut-pro/ver92bd100a/mac)
- [Apple: Intro to the Magnetic Timeline in Final Cut Pro for iPad](https://support.apple.com/it-it/guide/final-cut-pro-ipad/devd9b6a5715/ipados)
- [Wikipedia: iMovie](https://en.wikipedia.org/wiki/IMovie)
- [Engadget: Maybe iMovie '08 isn't such a bad change after all](https://www.engadget.com/2007-08-27-maybe-imovie-08-isnt-such-a-bad-change-after-all.html)
- [Joseph Dickerson: iMovie 08 – a step back?](https://www.josephdickerson.com/blog/2007/08/19/imovie-08-a-step-back/)
- [XDA: Apple iMovie 3.0 introduces Storyboards and Magic Movie](https://www.xda-developers.com/apple-imovie-3-features/)
- [Videomaker: iMovie 3.0 receives two major time-saving features](https://www.videomaker.com/news/imovie-3-0-receives-two-major-time-saving-features/)
