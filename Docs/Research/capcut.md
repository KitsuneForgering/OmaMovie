# CapCut: o que as pessoas gostam (e o que não gostam)

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Parte das fontes sobre CapCut é material promocional ou de
> concorrentes (ex.: VEED, Podcastle). Os números de preço e as reclamações
> devem ser lidos com isso em mente.

---

## 1. Resumo

O CapCut (ByteDance) é o editor mais popular entre criadores de vídeo curto.
As pessoas gostam dele por **velocidade até o resultado**:

1. **Templates** em que basta trocar os clips;
2. **legendas automáticas** editáveis;
3. **efeitos "de tendência"** prontos (speed ramps, transições, stickers);
4. **interface limpa**, de arrastar e soltar, que se aprende em minutos;
5. **multiplataforma** com sincronização (celular → desktop).

E não gostam de: **paywall agressivo** (preço do Pro dobrou em 2025),
**termos de uso** que dão à ByteDance licença perpétua sobre o conteúdo
(inclusive rascunhos), **instabilidade política** (banimentos) e **crashes com perda de trabalho**.

Para o OmaMovie, o CapCut mostra **quais operações de alto nível o público
quer**, e os pontos fracos dele são diferenciais óbvios: offline, sem conta,
sem paywall e com o conteúdo do usuário sob controle do usuário.

---

## 2. O que as pessoas gostam

### 2.1 Velocidade até o resultado
O argumento mais repetido: ir do material bruto a um vídeo pronto em minutos.
Templates, presets e automação existem para isso.

### 2.2 Templates
- Templates prontos em que o usuário **só troca fotos ou clips**.
- Templates seguem **tendências** (integração com o TikTok), o que facilita participar de "trends" e desafios virais.

### 2.3 Legendas automáticas
- Geradas por IA a partir do áudio e **editáveis** depois.
- Importantes porque muito vídeo em rede social é assistido **sem som**.
- É o recurso mais citado como "economiza muito tempo".

### 2.4 Speed ramps (curvas de velocidade)
- Speed > Curve com presets nomeados: **bullet, montage, jump cut, hero time, flash in, flash out**, todos customizáveis.
- Criam câmera lenta progressiva e acelerações sem keyframes manuais.

### 2.5 Outros recursos de alto nível
| Recurso | O que faz |
|---|---|
| Keyframes | Em "todas as configurações" |
| Auto Reframe | Detecta o sujeito e recompõe para outra proporção (ex.: horizontal → vertical) |
| Beat Sync | Alinha cortes à batida da música |
| Text-to-speech | Várias vozes e idiomas |
| Remoção de fundo | Sem tela verde |
| Chroma key, estabilização, motion tracking | Gratuitos na época em que o CapCut ganhou popularidade |
| Biblioteca de stickers, efeitos, transições, áudio | Conteúdo pronto |

### 2.6 Interface
- "Clean, straightforward", arrastar e soltar; começa-se a editar em minutos.
- Timeline multitrack convencional no desktop.
- **Mobile-first**: a versão de celular é a mais refinada; o desktop é descrito como menos polido.
- iOS, Android, web e desktop: começar no celular e terminar no computador.

---

## 3. O que as pessoas não gostam

| Problema | Detalhe |
|---|---|
| **Preço** | Em maio de 2025 o Pro subiu de US$ 9,99 para US$ 19,99/mês. Legendas dinâmicas, efeitos e armazenamento em nuvem foram para trás do paywall |
| **Cobrança e suporte** | Reclamações de cobrança após cancelamento e suporte lento, limitado a FAQ e e-mail |
| **Termos de uso (junho de 2025)** | Licença **perpétua, irrevogável, mundial e sem royalties** para a ByteDance sobre todo conteúdo enviado, **incluindo rascunhos não publicados**, mesmo após excluir a conta. Risco para agências e para conteúdo sob NDA |
| **Disponibilidade** | Banido na Índia desde 2020; nos EUA, banido em janeiro de 2025 e, segundo a review da VEED, indisponível para novos downloads nas lojas americanas |
| **Remoção de conteúdo** | Pode remover conteúdo sem aviso |
| **Estabilidade** | Relatos de crashes durante a edição com **perda de trabalho** |

---

## 4. Implicações para o OmaMovie

### 4.1 Diferenciais óbvios

O que o CapCut não oferece e o OmaMovie oferece por definição:

- **Offline e local**: mídia e projeto nunca saem da máquina.
- **Sem conta, sem termos sobre o conteúdo do usuário, sem paywall de recursos, sem marca d'água.**
- **Estabilidade e preservação do trabalho**: salvamento atômico e autosave
  (`CLAUDE.md` §14). Perda de trabalho é a reclamação mais grave em qualquer editor.

### 4.2 Recursos a considerar e impacto na arquitetura

| Recurso do CapCut | Recomendação | Impacto arquitetural |
|---|---|---|
| **Speed ramps com presets** | Sim, médio prazo | **O mapeamento tempo da timeline → tempo da mídia não pode ser só "velocidade constante".** Precisa suportar uma **curva** (função monotônica por partes), avaliada em `RationalTime`. Decidir no ADR-0002 (representação temporal) **antes** de implementar velocidade, mesmo que a UI inicial só tenha velocidade constante. Interpolação de frames (optical flow) é um passo posterior e separado |
| **Keyframes em tudo** | Sim | Já previsto. Parâmetros de efeitos e transforms devem ser animáveis por padrão, não por exceção |
| **Legendas como elemento de primeira classe** | Sim | Tipo de track de **legenda/texto** no modelo, com import/export SRT/VTT. Gerar legendas automaticamente exige transcrição, o que é um não objetivo de IA; se um dia entrar, preferir modelo **local** (offline) e manter o resultado como legendas editáveis comuns |
| **Mudar a proporção do projeto (vertical/horizontal)** | Sim | O canvas do projeto deve poder mudar de tamanho/proporção sem quebrar a montagem. Decidir no ADR do compositor se transforms são guardados em **coordenadas normalizadas** ou em pixels, e o que acontece ao trocar a proporção. Auto Reframe (detecção de sujeito) fica para depois |
| **Templates** | Futuro | Mesma conclusão do iMovie: template gera timeline comum e editável (`imovie.md` §7) |
| **Beat sync** | Futuro | Requer detecção de batida (análise de áudio, sem IA pesada) e marcadores na timeline. Marcadores devem existir no modelo |
| **Presets nomeados de efeitos/transições** | Sim | Presets são dados (parâmetros salvos) sobre efeitos genéricos, como os presets de overlay do iMovie |
| **Text-to-speech, remoção de fundo, stock** | Não agora | Não objetivos (IA/cloud) |
| **Sincronização multiplataforma** | Não | Não objetivo (cloud). Projeto em arquivo já permite sincronizar com ferramentas do próprio usuário |

### 4.3 Público

O CapCut mostra que o público de vídeo curto vertical é grande e quer
**operações de resultado** ("deixa em câmera lenta dramática"), não
**operações de mecanismo** ("adiciona keyframe de velocidade"). Para o
OmaMovie: o inspector oferece o preset por intenção primeiro e os parâmetros
depois. É o mesmo princípio das operações nomeadas do iMovie.

### 4.4 O que não copiar

- Biblioteca de "tendências" e conteúdo de stock (exige servidor).
- Interface mobile-first.
- Dependência de conta e nuvem.

---

## Fontes

- [App Store: CapCut](https://apps.apple.com/app/1500855883)
- [Microsoft Store: CapCut](https://apps.microsoft.com/detail/xp9kn75rrb9nhs)
- [CapCut: How to use CapCut (guia oficial)](https://capcut.com/resource/how-to-use-capcut)
- [The Nerd in Charge: Why I'm obsessed with CapCut](https://thenerdincharge.substack.com/p/dont-just-survivestorytell-it-why)
- [ABNewswire: CapCut is basically the cheat code for making videos](https://markets.financialcontent.com/lightport.lightport5/article/abnewswire-2025-12-30-capcut-is-basically-the-cheat-code-for-making-videos-that-dont-look-boring)
- [Skillademia: CapCut keyboard shortcuts](https://www.skillademia.com/shortcuts/capcut-shortcuts/)
- [VEED: CapCut review 2026](https://www.veed.io/learn/capcut-review)
- [Podcastle: CapCut's latest terms of service and pricing](https://podcastle.ai/blog/capcut-terms-of-service)
- [Fluxnote: CapCut questions answered (June 2026)](https://fluxnote.io/guides/capcut-questions-answered)
- [Wikipedia (espelho): CapCut](https://wiki.krisyotam.com/content/wikipedia_en_all_nopic_2026-03/CapCut)
