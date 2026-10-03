# Design da UI — OmaMovie

> Versão de 2026-10-02, com as decisões do §12. Base: `Docs/Research/imovie.md`
> (princípios), `movie-maker.md`, `capcut.md`, `davinci-resolve.md` §4 e
> `omarchy-integration.md`. Regras de implementação continuam no `CLAUDE.md` §11.
>
> Inspirado no iMovie **nos princípios**, sem copiar identidade visual, ícones,
> nomes de recursos ou trade dress da Apple.

---

## 1. Princípios

| # | Princípio | De onde vem |
|---|---|---|
| 1 | **Três áreas fixas**: biblioteca, viewer, timeline. Nada de painéis flutuantes ou workspaces | iMovie |
| 2 | **Controles aparecem por contexto**: a seleção decide o que se pode ajustar | iMovie (barra de ajustes), Clipchamp |
| 3 | **Manipular no preview**: posição, escala e crop arrastando na imagem; números ficam para precisão | iMovie |
| 4 | **Timeline magnética por padrão**: sem buracos acidentais; o modelo por baixo continua multitrack genérico | iMovie / FCP |
| 5 | **Presets por intenção**: "Picture-in-picture", não "camada + transform + máscara"; depois de aplicado, tudo continua editável | iMovie, CapCut |
| 6 | **Esconder, nunca remover**: o avançado está a um clique ou atalho, mas não ocupa a tela | Lição do iMovie '08 |
| 7 | **Teclado primeiro**: tudo acessível por atalho e por paleta de comandos | Cultura do Omarchy, Premiere |
| 8 | **A interface segue o tema do Omarchy; a imagem, nunca** | `omarchy-integration.md` §3–4 |
| 9 | **Funciona em meia tela**: no Hyprland a janela vive em tiling | Omarchy |

---

## 2. Layout

### 2.1 Janela larga (padrão, ≥ ~1500 px)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ◂ Projetos   Meu vlog ▾                       ⟲ ⟳     ⤓ Importar   ⇪ Exportar │  barra superior
├───────────────────────────────┬──────────────────────────────────────────────┤
│ BIBLIOTECA                    │  ◐ Cor  ⬚ Recorte  ♪ Volume  ⏱ Velocidade  ✦ Efeitos  ⧉ Sobrepor  ⓘ │  barra de ajustes
│ ┌───────────┐                 ├──────────────────────────────────────────────┤
│ │ Projeto   │ Mídia Títulos   │  [gaveta de controles do ajuste ativo]       │  (só quando há ajuste aberto)
│ │ Gravações●│ Transições Áudio├──────────────────────────────────────────────┤
│ │ Vídeos    │                 │                                              │
│ │ Favoritos │ ▢▢▢▢  ▢▢▢▢      │                 VIEWER                       │
│ │ + Pasta   │ ▢▢▢▢  ▢▢▢▢      │          (fundo neutro, sem tema)            │
│ └───────────┘ ▢▢▢▢  ▢▢▢▢      │                                              │
│               (skimming)      │   00:01:12:08 / 00:04:30:00     ◂◂  ▶  ▸▸   ⤢ │  transporte
├───────────────────────────────┴──────────────────────────────────────────────┤
│ ▁▂▃▅▇▅▃▂▁▁▂▃▅▃▂▁▁▂▃▅▇▅▃▂▁  ← minimapa (projeto inteiro, região visível)        │
│ ┊   T  [ Título "Bem-vindo" ]                                                │  camadas acima
│ ┊   V      [ PiP webcam ]                                                     │
│ ┊ ▶ [ clip 1 ][⋈][ clip 2      ][⋈][ clip 3 ][ clip 4  ]   ← trilha principal │  (magnética)
│ ┊ ♪ ~~~~~~~~~~ música ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                           │  áudio abaixo
│ ┊ 🎙   ~~ narração ~~                                                          │
│ ┊                                     🔍 ──●──   ⌁ snap                       │
└──────────────────────────────────────────────────────────────────────────────┘
```

- **Proporção padrão**: topo ~55%, timeline ~45%. **Um único divisor** horizontal ajusta tudo (iMovie).
- **Biblioteca** à esquerda, ~30% da largura; **viewer** à direita, o maior possível.
- A barra superior tem só: voltar para Projetos, nome do projeto, undo/redo, importar, exportar.

### 2.2 Meia tela (tiling, ~900–1500 px)

```
┌────────────────────────────────────────────┐
│ ◂  Meu vlog ▾                ⤓   ⇪          │
├────────────────────────────────────────────┤
│ ◐ ⬚ ♪ ⏱ ✦ ⧉ ⓘ                    ▤ Biblioteca│  ← biblioteca vira painel sobreposto (Ctrl+1)
├────────────────────────────────────────────┤
│                 VIEWER                     │
├────────────────────────────────────────────┤
│ minimapa                                   │
│ timeline                                   │
└────────────────────────────────────────────┘
```

- A biblioteca vira um **painel deslizante** sobre o viewer (atalho `Ctrl+1`), fechando ao adicionar o clip.
- Rótulos da barra de ajustes viram só ícones (com tooltip e atalho).

### 2.3 Janela estreita (< ~900 px)

Viewer em cima, timeline embaixo, biblioteca e controles como painéis sobrepostos. Utilizável, não otimizado.

---

## 3. Duas telas: Projetos e Edição

Como no iMovie, há uma tela de **Projetos** e a tela de **Edição**.

### 3.1 Qual tela abre (decidido)

Depende de **como** o app foi aberto:

| Origem | Abre em |
|---|---|
| Launcher, menu do Omarchy, `omamovie` sem argumentos | **Projetos** |
| Arquivo de projeto (duplo clique, "Abrir com", `omamovie projeto.<ext>`) | **Edição** desse projeto |
| Um ou mais vídeos ("Abrir com OmaMovie", `omamovie video.mp4`) | **Edição** de um projeto novo com os vídeos na timeline (formato pelo primeiro clip) |
| "Editar" numa gravação (tela de Projetos ou, no futuro, notificação do Omarchy) | **Edição** de um projeto novo com a gravação |
| App já aberto e recebe um arquivo | Mesma regra, na janela existente (instância única) |

Na tela de Edição, "◂ Projetos" sempre volta para a lista.

**Projetos**
```
┌──────────────────────────────────────────────────────────────┐
│  OmaMovie                                   + Novo projeto     │
│                                                              │
│  Recentes                                                    │
│  ▢ Meu vlog        ▢ Tutorial Hyprland    ▢ Short #12 (9:16)  │
│                                                              │
│  Gravações recentes do Omarchy                     ver todas ›│
│  ▢ 10:42 hoje      ▢ ontem 21:03          ▢ ontem 18:10      │
│     [ Editar ]                                               │
└──────────────────────────────────────────────────────────────┘
```
- **"Editar" numa gravação** cria um projeto já com ela na timeline: o atalho do fluxo gravar (ALT+PRINT) → editar.
- **Novo projeto** pergunta só uma coisa: formato (16:9 horizontal, 9:16 vertical, 1:1 quadrado), com padrão 16:9. Resolução e taxa de quadros vêm do primeiro clip (editável depois).

---

## 4. Biblioteca

- **Fontes** (lista à esquerda, recolhível): Projeto (mídia já usada), **Gravações** (pasta de vídeos do Omarchy, com indicador de novas), Vídeos, Favoritos, pastas adicionadas pelo usuário.
- **Abas de conteúdo**: Mídia, Títulos, Transições, Áudio (efeitos sonoros e músicas locais do usuário). Sem conteúdo de stock/nuvem.
- **Grade de miniaturas** com tamanho ajustável (`Ctrl+scroll`).
- **Skimming**: passar o mouse percorre o clip no viewer sem dar play. Com teclado: setas movem a seleção, `Espaço` toca.
- **Selecionar um trecho**: arrastar sobre a miniatura marca In/Out (ou `I`/`O`); a faixa marcada é o que vai para a timeline.
- Marcadores visuais: trecho já usado no projeto (barra fina), favorito, rejeitado.
- `F` favorita, `Delete` rejeita (não apaga o arquivo), `U` remove avaliação, `Ctrl+F` busca.

---

## 5. Viewer

- **Fundo neutro fixo** (cinza escuro), independente do tema do Omarchy.
- **Manipulação direta** quando um ajuste está ativo: alças de posição/escala/rotação (Transformar), retângulo de recorte (Recorte), conta-gotas (Cor).
- **Guias opcionais**: área segura, terços, contorno do formato (útil ao reenquadrar 16:9 → 9:16).
- **Transporte**: timecode atual/total, voltar, play, avançar, tela cheia. Botões grandes só na tela cheia; na janela, mínimos.
- **Indicador de tempo real**: um pequeno aviso aparece só quando o trecho não vai tocar em tempo real (efeito sem GPU, máquina fraca), com opção de pré-renderizar. Nada aparece quando está tudo bem.
- **Skimming na timeline** também mostra o frame no viewer (cabeça de leitura fantasma).

---

## 6. Barra de ajustes e gaveta de controles

A barra fica acima do viewer. **Só os ajustes que se aplicam à seleção ficam habilitados.** Um ponto (•) marca os ajustes já ativos no clip selecionado.

| Ícone | Ajuste | Controles na gaveta (nível 1) | "Mais" (nível 2) | Release |
|---|---|---|---|---|
| ◐ | **Cor** | Automático, exposição, contraste, saturação, temperatura | Rodas de cor, curvas, LUT | v0.1 básico, v0.3 avançado |
| ⬚ | **Recorte e enquadramento** | Ajustar / Preencher / Recortar; pan & zoom (Ken Burns) | Rotação livre, posição numérica | v0.1 |
| ♪ | **Volume** | Volume, fade in/out, silenciar | Equalizador, redução de ruído, normalizar | v0.1 básico |
| ⏱ | **Velocidade** | Lenta / Normal / Rápida, inverter | **Speed ramps com presets**, congelar quadro | v0.2 |
| ✦ | **Efeitos** | Filtros com miniatura de prévia | Parâmetros, keyframes | v0.2 |
| ⧉ | **Sobrepor** (só em camadas acima da principal) | Picture-in-picture, Lado a lado, Cutaway, Chroma key | Borda, sombra, máscara, blend | v0.2 |
| ⓘ | **Info** | Nome, duração, arquivo de origem, codec, caminho de decode | — | v0.1 |

- A **gaveta** abre entre a barra e o viewer, empurrando o viewer um pouco para baixo (o viewer encolhe, não é coberto).
- **"Mais"** expande a gaveta. Se o conteúdo for grande (curvas, keyframes), abre um **painel lateral** no lugar da biblioteca, com "◂ Voltar à biblioteca".
- Para títulos, a barra muda: **Texto** (fonte, tamanho, cor, alinhamento), **Estilo** (presets), **Animação**.
- Para transições: **Tipo** e **Duração**.

---

## 7. Timeline

### 7.1 Estrutura visível
- **Trilha principal** (magnética): a sequência do filme. Remover um clip fecha o espaço; arrastar empurra os vizinhos.
- **Camadas acima** (vídeo, títulos): aparecem quando se arrasta algo para cima da trilha principal. Ficam **ligadas** ao clip da principal que está embaixo e se movem com ele.
- **Áudio abaixo** (música, narração, efeitos): também aparecem conforme necessário.
- **Sem cabeçalhos de track por padrão.** Uma preferência "Mostrar controles de trilha" exibe cabeçalhos com mute/solo/lock/ocultar (nível 3).
- O modelo é multitrack genérico (`CLAUDE.md` §10); "principal + ligadas" é como a UI apresenta e edita.

### 7.2 Elementos
- **Minimapa** no topo: o projeto inteiro em miniatura, com a região visível destacada (resposta à timeline dupla do Resolve). Clicar ou arrastar navega.
- **Transições** como um pequeno ícone ⋈ **entre** dois clips (como o Movie Maker), clicável para editar.
- **Waveform** na parte de baixo de cada clip com áudio.
- **Zoom centrado no mouse** (`Ctrl+scroll`, `Ctrl+=`/`Ctrl+-`), `Shift+Z` ajusta o projeto à largura. No zoom mínimo, os clips viram miniaturas de tamanho igual (**storyboard**, v0.2).
- **Snapping** ligado por padrão (`N` alterna).

### 7.3 Edição

Com o mouse, a edição funciona sem trocar de ferramenta (como no iMovie).
Para quem vem do Final Cut, as **ferramentas** existem e trocam por tecla (§8.1).

| Gesto (ferramenta Seleção) | Resultado |
|---|---|
| Arrastar borda do clip | Trim com ripple (magnético) |
| Duplo clique na junção | **Editor de corte**: os dois lados com o material não usado esmaecido; ajusta o corte e o split de áudio (J/L-cut) |
| Arrastar borda com a ferramenta Trim (`T`) | Roll entre vizinhos |
| Arrastar o meio com a ferramenta Trim | Slip (muda o conteúdo, mantém posição e duração) |
| Clicar com a ferramenta Lâmina (`B`) | Divide naquele ponto |
| Ferramenta Posição (`P`) | Move sem magnetismo (sobrescreve, pode deixar espaço) |

Os atalhos de edição estão no §8.1.

---

## 8. Teclado e paleta de comandos

### 8.1 Atalhos (decidido: convenção iMovie/Final Cut)

Regra de tradução: **`Cmd` do macOS vira `Ctrl`**, `Option` vira `Alt`. O
OmaMovie não usa `SUPER` (reservado ao Hyprland/Omarchy).

**Reprodução e navegação**
| Atalho | Ação |
|---|---|
| `Espaço` | Play/pausa |
| `J` `K` `L` | Trás / pausa / frente (repetir acelera) |
| `/` | Tocar a seleção |
| `←` `→` | Quadro anterior/próximo |
| `Shift+←` `→` | 10 quadros |
| `↑` `↓` | Corte anterior/próximo |
| `Home` / `End` | Início / fim do projeto |
| `S` | Liga/desliga skimming |
| `N` | Liga/desliga snapping |

**Edição**
| Atalho | Ação |
|---|---|
| `I` / `O` | Marcar entrada/saída |
| `X` | Selecionar o clip inteiro sob o skimmer como intervalo |
| `E` | Adicionar a seleção da biblioteca ao fim |
| `W` | Inserir na cabeça de leitura |
| `Q` | Conectar como camada acima, na cabeça de leitura |
| `D` | Sobrescrever na cabeça de leitura |
| `Ctrl+B` | Dividir na cabeça de leitura |
| `Delete` | Remover fechando o espaço |
| `Shift+Delete` | Substituir por espaço vazio |
| `Ctrl+D` | Mudar duração |
| `M` | Adicionar marcador |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Desfazer / refazer |

**Ferramentas**
| `A` Seleção | `B` Lâmina | `T` Trim | `P` Posição | `R` Intervalo |
|---|---|---|---|---|

**Biblioteca**
| Atalho | Ação |
|---|---|
| `F` | Favoritar |
| `Delete` (na biblioteca) | Rejeitar (não apaga o arquivo) |
| `U` | Remover avaliação |
| `Ctrl+F` | Buscar |

**Janela e projeto**
| Atalho | Ação |
|---|---|
| `Ctrl+=` / `Ctrl+-` / `Shift+Z` | Zoom da timeline / ajustar ao projeto |
| `Ctrl+1` | Mostrar/ocultar biblioteca |
| `Tab` | Alterna foco: biblioteca → viewer → timeline |
| `Ctrl+I` | Importar |
| `Ctrl+E` | Exportar |
| `Ctrl+Shift+F` | Viewer em tela cheia |
| `Ctrl+K` | **Paleta de comandos** |

- Todos os atalhos vêm do sistema central de ações e serão remapeáveis.
- **Verificar** cada atalho contra a documentação oficial do iMovie e do Final
  Cut Pro antes de implementar; esta tabela foi montada de memória da convenção.
- `Ctrl+K` é o editor de palavras-chave no Final Cut; o OmaMovie não tem palavras-chave, então a paleta ocupa o atalho.

### 8.2 Paleta de comandos (`Ctrl+K`)
- Lista **todas** as ações do app, buscáveis por nome ("velocidade 50%", "exportar 1080p", "adicionar marcador").
- É o "nível 3" de revelação: qualquer capacidade é alcançável sem botão na tela.
- Combina com o estilo do Omarchy (launcher e menus por busca).

---

## 9. Revelação progressiva

| Nível | Onde | Exemplo |
|---|---|---|
| **1 — sempre visível** | Barra de ajustes, gaveta, gestos na timeline | Recorte, volume, fade, split |
| **2 — um clique** | "Mais" na gaveta, painel lateral, editor de corte | Curvas, equalizador, speed ramps, keyframes |
| **3 — sob demanda** | Paleta de comandos, preferências, cabeçalhos de trilha | Roll/slide numéricos, edição de 3 pontos, mute/solo/lock |

Não há "modo simples" e "modo pro" separados: um modo esconde capacidade e obriga a trocar de contexto.

---

## 10. Linguagem visual

- **Cores**: tokens semânticos alimentados pelo `colors.toml` do Omarchy.

  | Token da UI | Cor do Omarchy |
  |---|---|
  | Fundo da janela | `background` |
  | Painéis (biblioteca, gaveta) | `dark_background` |
  | Fundo da timeline | `darker_background` |
  | Elementos elevados (clips, botões) | `lighter_background` |
  | Seleção, cabeça de leitura, foco | `accent` |
  | Texto / texto secundário | `foreground` / `dark_foreground` |
  | Clips por tipo (vídeo, áudio, título) | `blue` / `green` / `magenta` (misturados com o fundo) |
  | Avisos / erros | `yellow` / `red` |

- **Viewer, miniaturas e scopes sem tema**: mostram a imagem real em fundo neutro.
- **Modo claro**: segue `mode` do tema (há temas claros como `catppuccin-latte` e `flexoki-light`).
- **Densidade**: plana, poucas bordas, separação por tom de fundo e espaçamento; cantos levemente arredondados.

### 10.1 Tipografia (decidido)

- **Fonte do Omarchy em toda a UI**, lida com `omarchy-font-current` e atualizada
  ao vivo pelo hook/watcher de troca de fonte. Fallback fora do Omarchy: a fonte monoespaçada padrão do sistema (`fontconfig`).
- Consequências de usar uma fonte geralmente monoespaçada:
  - rótulos ocupam mais largura: preferir rótulos curtos e ícones com tooltip na barra de ajustes;
  - timecodes e números alinham naturalmente (vantagem);
  - hierarquia por **tamanho, peso e cor** (`foreground` / `dark_foreground`), não por troca de família;
  - testar com fontes de larguras diferentes (as que o Omarchy oferece) para não quebrar layouts.
- Tamanho base derivado do `base-size` do Omarchy (`~/.config/omarchy/shell.toml`, `[font]`) quando existir **(verificar se é a fonte certa para apps)**.

### 10.2 Ícones (decidido)

- **Conjunto próprio em SVG**, desenhado para o OmaMovie (sem copiar ícones da Apple ou de outros editores).
- Regras do conjunto:
  - grade de 24×24 px, traço único (ex.: 1,5 px), cantos e terminações consistentes;
  - **monocromáticos**: uma única cor, aplicada em runtime a partir do tema (cor normal, `accent` quando ativo, `dark_foreground` quando desabilitado);
  - variante preenchida só para estados ativos (ex.: favorito);
  - legíveis em 16 px.
- Implementação: SVGs em `apps/omamovie/resources/icons/`, carregados via Qt Resource System; colorização
  pelo mecanismo de ícones do Qt Quick Controls (`icon.source` + `icon.color`) **(verificar comportamento com SVG monocromático no Qt 6.11)**.
- Licença: própria do projeto (MIT), arquivo de créditos se algum ícone derivar de outro conjunto livre.
- Os glyphs usados nos wireframes deste documento (◐ ⬚ ♪ ...) são **apenas marcadores**.
- **Janela opaca** (regra do Hyprland documentada; `omarchy-integration.md` §4).

---

## 11. O que é diferente do iMovie (de propósito)

| iMovie | OmaMovie | Por quê |
|---|---|---|
| Camadas limitadas, sem keyframes gerais | Multitrack completo, keyframes (v0.2), escondidos até serem necessários | Esconder, não remover |
| Orientado ao mouse | Teclado completo + paleta de comandos | Público do Omarchy |
| Sem visão geral da timeline | Minimapa | Pesquisa do Resolve (timeline dupla) |
| Biblioteca ligada ao app Fotos | Pastas locais + Gravações do Omarchy | Plataforma |
| Um visual fixo | Tema do Omarchy, claro/escuro | Integração |
| Editor separado (FCP) para quem cresce | O mesmo app, revelação progressiva | Tese do projeto |
| Formatos horizontais | 16:9, 9:16, 1:1 desde o novo projeto | Criadores de vídeo curto (CapCut) |

---

## 12. Decisões tomadas (2026-10-02)

| # | Tema | Decisão |
|---|---|---|
| 1 | Controles de ajuste | **Gaveta acima do viewer** (§6) |
| 2 | Tipografia | **Fonte do Omarchy** em toda a UI (§10.1) |
| 3 | Ícones | **Conjunto vetorial próprio em SVG** (§10.2) |
| 4 | Atalhos | **Convenção iMovie/Final Cut**, `Cmd`→`Ctrl` (§8.1) |
| 5 | Tela inicial | **Depende da origem**: launcher abre Projetos; arquivo de projeto ou mídia abre a Edição (§3.1) |

Ainda em aberto: nenhum ponto bloqueante. Ajustes finos virão do protótipo.

## 13. Próximo passo

Mockup navegável (HTML, dados fictícios): `Docs/mockups/ui-mockup.html`.

Depois das respostas: um **protótipo visual** do layout (mockup navegável) e,
em seguida, um protótipo em QML dentro do M6, com dados falsos, para validar
tamanhos, densidade e o comportamento em meia tela antes de ligar o motor.
