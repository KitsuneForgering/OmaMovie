# CLAUDE.md — OmaMovie

Guia operacional para agentes de programação que trabalham neste repositório.
Ele define regras de engenharia, invariantes arquiteturais, prioridades e
convenções. Não é documentação de usuário.

> **UI simples, pipeline sério.**
> A UI pode esconder complexidade. A arquitetura não pode fingir que ela não existe.

---

## 0. Estado atual do repositório

- **M0 concluído** (2026-10-02): build com GNU Make, `libs/base` (tempo racional, `Result`/`Error`,
  logging, job system), testes com Cest, CI e ADRs 0001–0003. As demais libs, o app e as
  ferramentas ainda não existem.
- A estrutura, os comandos e as decisões abaixo são o **alvo**. Ao criar algo
  que este documento descreve, siga o que está aqui. Ao divergir, registre o
  motivo em um ADR (§20) e atualize este arquivo no mesmo commit.
- Mantenha a seção §3 (Comandos) sincronizada com a realidade. Comando
  documentado que não funciona é bug.
- O que construir e em que ordem: `Docs/implementation-plan.md`. A pesquisa
  que embasa as decisões fica em `Docs/Research/`.

---

## 1. Identidade e nomenclatura

| Item | Nome |
|---|---|
| Produto | `OmaMovie` |
| Repositório | `oma-movie` |
| Executável principal | `omamovie` |
| Bibliotecas reutilizáveis | `oma-media`, `oma-timeline`, `oma-compositor`, `oma-project`, … |
| CLI de projetos | `oma-project` |
| Namespace C++ raiz | `oma::` (sub-namespaces por módulo: `oma::media`, `oma::timeline`, …) |

**Proibido:** `OmaVideo`, `omavideo`, `oma-video` em qualquer lugar (código, docs, nomes de arquivo, mensagens de log).

Plataforma-alvo: **Omarchy** (Arch Linux + Hyprland, Wayland). O projeto **não é
multiplataforma**. Não adicione camadas de portabilidade para Windows/macOS,
nem caminhos X11. É permitido assumir toolchain recente (GCC/Clang atuais do
Arch), Qt 6 recente, Mesa/NVIDIA recentes, PipeWire e Wayland.

---

## 2. Escopo

### Objetivos
Edição não destrutiva · timeline multitrack · composição GPU (Vulkan) ·
playback em tempo real · pipeline GPU-first · formatos modernos · importação
de projetos de outros NLEs · arquitetura modular · integração com Omarchy ·
UI simples e contextual · crescimento sem reescrever o núcleo.

### Não objetivos (até decisão explícita em contrário)
- clone do Premiere ou compositor estilo After Effects;
- motion graphics completo, multicam avançado, tracking complexo;
- IA sem caso de uso concreto;
- cloud, colaboração remota;
- **sistema de plugins de terceiros** antes da arquitetura estabilizar;
- escrita de formatos proprietários de terceiros antes do parser estar bem compreendido.

Se uma tarefa empurrar para um não objetivo, pare e pergunte.

---

## 3. Comandos

Build com GNU Make (ADR-0001). `make help` lista tudo.

```sh
make -j                        # libs + testes + compile_commands.json (BUILD=debug)
make -j test                   # compila e roda os testes
make -j test FILTER=rescale    # só testes cujo nome contém o padrão
make -j BUILD=asan test        # AddressSanitizer + UBSan
make -j BUILD=tsan test        # ThreadSanitizer
make -j BUILD=release test     # -O2
make -j CXX=clang++ test       # outro compilador (a CI roda g++ e clang++)
make format | make format-check
make tidy                      # clang-tidy nas libs (warnings são erros)
make fixtures                  # mídia de teste em tests/fixtures/generated/ (ffmpeg)
```

Commits e releases:

```sh
scripts/check-commits.sh origin/master..HEAD   # valida Conventional Commits antes do push
git tag v0.1.0 && git push origin v0.1.0       # dispara o workflow de release
```

- **Conventional Commits em inglês**, imperativo, sem ponto final, até 100 caracteres:
  `<tipo>(<escopo>): <descrição>`. Tipos: `feat fix docs style refactor perf test build ci chore revert`.
  Escopo = lib ou área (`base`, `media`, `timeline`, `ui`, `research`, `adr`...). `!` marca breaking change.
- **Microcommits**: um commit por mudança coerente que compila e passa nos testes sozinho.
  Não misturar refatoração com funcionalidade nem docs com código no mesmo commit.
- A CI valida as mensagens (`.github/workflows/commits.yml`), compila e testa em g++/clang++ ×
  debug/asan/tsan/release (`ci.yml`). Tags `vX.Y.Z` geram a release com notas montadas dos
  commits (`release.yml`, `scripts/changelog.sh`).

Comandos-alvo, ainda não existem:

```sh
# Executar
./build/debug/apps/omamovie/omamovie

# CLI de projetos
./build/debug/tools/oma-project/oma-project inspect <arquivo>

# Benchmarks
./build/release/tools/bench/oma-bench --scenario <nome> --json out.json
```

Antes de declarar uma tarefa concluída: `make test` passa sem warnings novos, e mudanças
concorrentes passam também em `BUILD=tsan`. Se não rodou os testes, diga isso.

---

## 4. Stack

| Camada | Tecnologia | Observação |
|---|---|---|
| Core | C++23 | `std::expected`, `std::span`, concepts. Compiladores do Arch suportam. |
| Build | GNU Make não recursivo (`Makefile` + `module.mk` por lib) | ADR-0001. **Não usar CMake.** |
| Testes | Cest (`third_party/cest`, header-only) | ADR-0001; regras em §25. |
| UI | Qt 6 Quick / QML | Apenas apresentação e interação. |
| Mídia | FFmpeg (libavformat/codec/util/filter/swresample) | Encapsulado em `oma-media`. |
| GPU / composição | Vulkan | Backend gráfico principal. |
| Compute | Vulkan Compute, CUDA, CPU | Atrás de `ComputeBackend`. **Sem OpenCL** (§9.3). |
| Áudio | PipeWire | Integração de sistema. **Não usar OpenAL** como base. |
| Dados estruturados | SQLite | Só onde justificado (§17). |
| Cripto | OpenSSL | Só com necessidade criptográfica real. Hashing de conteúdo não exige OpenSSL por si só. |

**Dependências novas:** qualquer dependência não listada aqui exige
justificativa no commit/PR (problema resolvido, alternativa considerada,
custo de build/runtime). Dependência grande exige ADR. Não adicione framework
para resolver problema trivial.

Fuzzing com libFuzzer (Clang).

---

## 5. Arquitetura de módulos

### 5.1 Layout

```
oma-movie/
  apps/
    omamovie/            # Executável Qt Quick. Composição de tudo. QML + bridge C++.
  libs/
    base/                # Result/Error, logging, time (RationalTime), utilitários. Sem Qt.
    gpu/                 # Device Vulkan, memória, sync, interop (DMA-BUF, CUDA). Sem Qt.
    media/               # oma-media: probe, demux, decode, encode, mux, frames, hwaccel.
    audio/               # Mixer, resampling, clock, saída PipeWire.
    compositor/          # Render graph + compositor Vulkan + efeitos.
    timeline/            # Modelo da timeline, operações de edição, undo/redo.
    project/             # Formato nativo, serialização, migração.
    project-ir/          # ProjectIR: representação intermediária neutra.
    importers/           # Um subdiretório por formato externo -> ProjectIR.
  tools/
    oma-project/         # CLI: inspect, dump, diff, validate, convert.
    bench/               # Benchmarks de pipeline.
  tests/
    <lib>/               # Testes Cest de cada lib (module.mk + main.cpp + test_*.cpp).
    support/             # oma_test.hpp: ponto de entrada do Cest para os testes.
    fixtures/            # Fixtures pequenas e reproduzíveis + scripts geradores.
  third_party/           # Código de terceiros vendorizado e fixado (cest/).
  Makefile               # Build; cada lib tem libs/<lib>/module.mk.
  Docs/
    adr/                 # Architecture Decision Records.
    Research/            # Pesquisa de produto/UX (não normativa).
```

`libs/base` e `libs/gpu` não estavam no esboço original: `base` evita que
tempo/erros/logging sejam duplicados ou acoplados a Qt; `gpu` permite que
compositor, compute e interop de decode compartilhem o mesmo `VkDevice` sem
que `media` dependa de `compositor`. A estrutura pode mudar com justificativa
técnica registrada.

### 5.2 Regras de dependência (invariantes)

```
base  <-  gpu  <-  media  <-  compositor
base  <-  audio
base  <-  timeline           (timeline NÃO depende de gpu, media concreta, compositor ou Qt)
base  <-  project-ir         (project-ir NÃO depende de timeline, project ou importers)
project-ir <- importers      (importers NÃO dependem do modelo interno do OmaMovie)
timeline, project-ir <- project
tudo  <-  apps/omamovie, tools/*
```

- **Qt só em `apps/` e em código de bridge explicitamente marcado.** As libs
  (`base`, `gpu`, `media`, `timeline`, `project-ir`, `project`, `importers`)
  não incluem headers Qt. Isso mantém `oma-media` separável e testável sem UI.
  Exceção possível: integração Qt RHI ↔ Vulkan, que vive no app (§9.4).
- **Nenhum include de Vulkan, CUDA ou FFmpeg em headers públicos de
  `timeline`, `project`, `project-ir` ou `importers`.**
- Headers FFmpeg (`libav*`) aparecem apenas dentro de `libs/media` (e em
  `tools/` se necessário para diagnóstico).
- Ciclos de dependência entre libs são proibidos. Se aparecer um, o desenho está errado.
- Cada lib expõe API pública em `include/oma/<modulo>/` e esconde implementação em `src/`.

---

## 6. Representação temporal (invariante crítico)

**Nunca use `float`/`double` como representação principal de tempo.** Doubles
são permitidos apenas para exibição na UI e para valores derivados descartáveis.

Tipos em `libs/base` (nomes-alvo):

- `Rational { int64_t num; int64_t den; }` — sempre normalizado, `den > 0`.
- `RationalTime { int64_t value; Rational timebase; }` — instante.
- `TimeRange { RationalTime start; RationalTime duration; }` — intervalo semiaberto `[start, start+duration)`.
- `FrameRate` (racional: 30000/1001, não 29.97) e `SampleRate` como tipos distintos.

Regras:
1. **Conversões são explícitas** e especificam arredondamento (`floor`,
   `ceil`, `nearest`). Sem conversão implícita entre timebases.
2. Rescale com aritmética de 128 bits (`__int128`) ou equivalente; detectar overflow, nunca truncar silenciosamente.
3. Comparar tempos de timebases diferentes só via função que faz a conversão exata.
4. **Não assumir CFR.** Busca de frame é por PTS, nunca por `index * frame_duration`
   em mídia arbitrária. VFR é caso normal.
5. Áudio é endereçado em amostras (sample-accurate). Edição de áudio não pode
   ser quantizada à grade de frames de vídeo.
6. A timeline tem timebase própria (do projeto/sequência). Cada clip mapeia
   tempo da timeline → tempo da mídia via conversão explícita (incluindo velocidade).
7. Timestamps vindos de FFmpeg (`AVRational`, `AV_NOPTS_VALUE`) são
   convertidos na borda de `oma-media`; `AV_NOPTS_VALUE` vira `std::optional` vazio, não um número mágico.

Testes de conversão temporal são obrigatórios e devem cobrir: 23.976/29.97/59.94,
44.1k/48k/96k, valores grandes (horas de mídia em timebase de 90 kHz), arredondamento e overflow.

---

## 7. Pipeline GPU-first

O compositor GPU é parte da arquitetura fundamental, não otimização futura.

### 7.1 Caminho-alvo

```
mídia comprimida -> demux -> decode HW -> superfície GPU -> compositor Vulkan -> efeitos -> preview
                                                                          \-> encode HW (export)
```

### 7.2 Anti-padrão proibido em caminhos de playback/export

```
decode CPU -> upload -> processa -> download -> upload de novo
```

Qualquer readback GPU→CPU no caminho quente precisa de justificativa explícita
em comentário e deve ser visível em profiling/log da categoria `gpu`.

### 7.3 Decode por fornecedor (investigar e validar antes de fixar)

| Hardware | Caminho preferido | Interop com Vulkan |
|---|---|---|
| Intel / AMD | VA-API via FFmpeg hwaccel, ou Vulkan Video | Export DMA-BUF → import Vulkan (`VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`) |
| NVIDIA | NVDEC (FFmpeg `cuda` hwaccel), ou Vulkan Video | CUDA ↔ Vulkan external memory/semaphores (`VK_KHR_external_memory_fd`, `cuImportExternalMemory`) |
| Qualquer | FFmpeg Vulkan hwaccel (Vulkan Video decode) | Nativo, se o driver suportar o codec |
| Fallback | Decode software | Upload único para a GPU, via staging buffer reutilizado |

- Detectar capacidades em runtime (codec × perfil × resolução × bit depth × driver) e registrar no log o caminho escolhido e por quê.
- Fallback para software é obrigatório e deve funcionar; o caminho HW não é o único caminho.
- Encode: VA-API, NVENC, Vulkan Video encode quando maduro; fallback software (ex.: libx264/libx265/SVT-AV1).

### 7.4 Cor

- Conversão YUV→RGB no shader, respeitando matrix, range (limited/full),
  primaries e transfer do stream. Nunca assumir BT.709 limited sem ler os metadados.
- Suportar 10-bit (P010 e similares) desde o desenho dos formatos de frame.
- Espaço de trabalho do compositor (ex.: linear em float16) é decisão de ADR.
  Blending deve ocorrer no espaço definido nele, não "onde funcionar".

---

## 8. oma-media

Camada reutilizável, separável do app. Responsável por: probing, demux,
decode, encode, mux, abstração de frame GPU/CPU, lifetime e sincronização de
frames, aceleração de hardware, conversão.

Regras:
- **Nenhuma chamada FFmpeg fora de `libs/media`.**
- Abstrações devem ter valor arquitetural (lifetime, threading, seleção de
  backend, tipos fortes). **Não** criar wrappers de uma linha que só renomeiam
  funções do FFmpeg. Dentro de `libs/media` é aceitável usar a API FFmpeg diretamente.
- Recursos FFmpeg são RAII: `unique_ptr` com deleters para `AVFormatContext`,
  `AVCodecContext`, `AVFrame`, `AVPacket`, `AVBufferRef`.
- Erros FFmpeg são convertidos em `oma::Error` com o código e `av_strerror` preservados.
- Probing retorna uma estrutura própria (streams, codec, dimensões, timebase,
  duração, cor, rotação/display matrix, layout de canais), não `AVStream*`.

### 8.1 Frames e lifetime

- `Frame` é um handle com **ownership explícito** e contagem de referências
  controlada (pool), não `shared_ptr` espalhado. Ao liberar, volta para o pool.
- Um frame GPU carrega: imagem(ns) Vulkan / memória importada, formato,
  espaço de cor, PTS (`RationalTime`) e **primitiva de sincronização**
  (timeline semaphore Vulkan) que indica quando está pronto e quando pode ser reutilizado.
- Nenhum frame pode sobreviver ao device/pool que o criou. Shutdown destrói na ordem: consumidores → pools → device.
- Sem `vkDeviceWaitIdle`/`vkQueueWaitIdle` no caminho quente. Sincronize por semáforos/fences específicos.
- Frames decodificados em HW mantêm referência ao `AVFrame`/surface de origem enquanto a imagem importada estiver em uso.

---

## 9. Compositor, render graph e backends

### 9.1 Camadas (invariante)

```
Timeline (modelo)  ->  RenderGraph (descrição por instante)  ->  Compositor  ->  Backend gráfico (Vulkan)
```

- A timeline **não conhece Vulkan**. Ela produz, para um tempo `t`, uma
  descrição declarativa (layers, transforms, crop, opacidade, blend, efeitos com parâmetros avaliados).
- O render graph é dados puros e testável sem GPU (teste de "que grafo sai para este estado da timeline").
- O compositor traduz o grafo em comandos para o backend, gerencia pipelines,
  descriptor sets e recursos transitórios.

### 9.2 Responsabilidades do compositor
Layers, scale, crop, rotation, translation, alpha blending, masks,
transitions, LUTs, color transforms, overlays, títulos, blend modes.

### 9.3 Compute

```cpp
class ComputeBackend;      // interface
// VulkanComputeBackend, CudaBackend, CpuBackend
```

- **Efeitos não dependem diretamente de CUDA.** Um efeito declara o
  que precisa; o backend executa.
- `CpuBackend` existe como referência de corretude e para testes (comparação de saída com tolerância).
- **Vulkan Compute é o backend genérico**: todo efeito tem implementação Vulkan.
- CUDA só onde houver ganho medido em NVIDIA.
- **OpenCL não é usado.** Motivos: o Omarchy não instala runtime OpenCL, e o
  rusticl (Mesa) não tem interop sem cópia com Vulkan, o que forçaria cópias de
  frame (viola §7). Ver `Docs/Research/opencl.md`. Não adicionar OpenCL sem
  decisão explícita e ADR.
- Não implemente os três backends de uma vez. Interface primeiro, um backend real, CPU para testes; os demais quando houver motivo.

### 9.4 Preview em Qt Quick
- Qt Quick deve rodar com o backend RHI **Vulkan**, compartilhando ou
  importando imagens do compositor sem readback para CPU.
- Investigar e registrar em ADR: device compartilhado (Qt usando o `VkDevice`
  do OmaMovie) vs. devices separados com memória externa. Preferir o que evita cópias e simplifica sincronização.
- O item QML de preview apenas exibe a textura final. Ele não decide o que renderizar.

### 9.5 Pipelines e shaders
- Shaders em GLSL ou Slang compilados para SPIR-V no build. Pipeline cache persistido (cache invalidável, §15).
- Nenhuma criação de pipeline durante playback se puder ser feita antes (pré-aquecimento).

---

## 10. Timeline

Componente central. Suporte progressivo a: múltiplas tracks de vídeo e áudio,
clips, in/out, split, trim, ripple, roll, slip, slide, snapping, transições,
keyframes, velocidade, estruturas aninhadas (quando justificadas), undo/redo.

Regras:
- Modelo da timeline em C++ puro (`libs/timeline`), sem Qt, sem GPU, sem FFmpeg.
- **Toda edição é um comando** (`Command` com `apply`/`revert`), aplicado via
  um único ponto de entrada. Undo/redo nasce com o modelo, não é adicionado depois.
- Operações compostas (ex.: ripple delete) formam uma transação única no histórico.
- Invariantes verificáveis por função (`validate()`) e testadas: sem sobreposição
  indevida em uma track, ranges válidos, referências de mídia existentes.
- Edição é não destrutiva: clips referenciam mídia e ranges, nunca modificam arquivos de origem.
- Clips referenciam mídia por ID estável, não por ponteiro nem caminho.
- **Estado de renderização nunca fica em componentes QML da timeline.** A UI
  lê um modelo exposto (`QAbstractItemModel`/propriedades) e emite intenções de edição.

---

## 11. UI (Qt Quick / QML)

Inspirada na simplicidade do iMovie, sem copiar identidade visual ou comportamento proprietário.
**Design de referência e decisões: `Docs/ui-design.md`** (layout, gaveta de ajustes,
timeline, atalhos, tema). Divergir dele exige atualizar o documento.

Decisões fixas (2026-10-02):
- Controles de ajuste numa **gaveta acima do viewer**, não num painel lateral fixo.
- **Fonte do Omarchy** em toda a UI (`omarchy-font-current`, com fallback).
- **Ícones SVG próprios**, monocromáticos e coloridos pelo tema. Não usar Nerd Font nem ícones de terceiros sem licença compatível.
- Atalhos na **convenção iMovie/Final Cut** (`Cmd`→`Ctrl`); nunca usar `SUPER`.
- Tela inicial **conforme a origem**: launcher → Projetos; arquivo de projeto ou mídia → Edição. Instância única.

Estrutura: `MediaPanel`, `PreviewPanel`, `TimelineView`, `Inspector`, `TransportControls`.

- Preview grande, timeline clara, biblioteca visual, poucos controles permanentes.
- **Inspector contextual**: a seleção determina o conteúdo.
  - Vídeo: transform, crop, opacity, speed, color, effects.
  - Áudio: gain, fades, EQ, processing.
  - Texto: font, size, layout, animation.
- Ferramentas avançadas aparecem por contexto, não como painéis sempre visíveis.
  Esconder complexidade ≠ remover capacidade.
- Integração visual com o tema ativo do Omarchy (mecanismo documentado em
  `Docs/Research/omarchy-integration.md`); isole essa leitura em um único ponto.
  Viewer, miniaturas e scopes **nunca** recebem cor do tema.
- Wayland nativo. Não usar APIs X11.

### 11.1 QML vs C++
QML: layout, interação, animação de UI, binding de propriedades.
**Proibido em JavaScript/QML:** decode, modelo complexo da timeline, render
graph, composição, sincronização, persistência, parsing de projetos.
Lógica não trivial em JS dentro do QML é sinal de que falta um modelo em C++.

### 11.2 Atalhos
Edição completa por teclado é requisito. Planejar desde cedo:
split, delete, ripple delete, undo/redo, frame step (±1 frame, ±N frames), J/K/L
(incluindo velocidades múltiplas por toques repetidos), in/out, seleção de clip próximo/anterior.
- Ações são registradas em um sistema central de ações (id, nome, atalho padrão, handler), não em `Keys.onPressed` espalhados.
- Atalhos devem ser remapeáveis no futuro; não hardcode em componentes.

---

## 12. Áudio

- PipeWire é a integração principal. Não usar OpenAL como base.
- **Durante playback, o relógio mestre é o áudio** (clock derivado das amostras
  efetivamente consumidas pelo dispositivo). Vídeo sincroniza com ele: descarta ou repete frames, nunca "estica" o áudio.
  Sem áudio, usar clock monotônico.
- O callback de tempo real do PipeWire **não aloca, não bloqueia, não faz log
  síncrono, não toma mutex disputado**. Comunicação via filas lock-free/ring buffers pré-alocados.
- Cobrir: sample rate do projeto vs. mídia vs. dispositivo, resampling
  (libswresample ou equivalente, em `media`/`audio`), volume, fades, mixing, waveform, latência reportada pelo dispositivo.
- Processamento interno em float32 planar (decisão a confirmar em ADR).

---

## 13. Concorrência

Nunca bloquear a thread de UI com: decode, encode, thumbnails, waveform,
probing, cache, proxies, imports, salvamento grande.

Modelo de threads (alvo, formalizar em ADR):

| Thread / pool | Responsabilidade |
|---|---|
| UI (main) | QML, eventos, despacho de comandos de edição |
| Render (Qt) | Scene graph, apresentação do preview |
| Playback | Clock, agendamento de frames, pedidos ao decode/compositor |
| Decode workers | Um pipeline por stream ativo, com fila limitada |
| Áudio RT | Callback PipeWire (regras de §12) |
| Job pool | Tarefas em background: probe, thumbnails, waveforms, proxies, imports, export |

Regras:
- **Sem `std::thread` ad hoc em componentes.** Toda tarefa assíncrona passa pelo job system (`libs/base`).
- Toda tarefa longa suporta **cancelamento** (token cooperativo) e **progresso**.
- Ownership de dados passados entre threads é explícito (move, handle de pool, ou imutável compartilhado). Documentar qual.
- Filas entre estágios são **limitadas** (backpressure). Nada de filas que crescem sem limite.
- Shutdown seguro: cancelar jobs → drenar filas → parar workers → liberar GPU. Testar o shutdown no meio do playback e de um import.
- Callbacks para a UI chegam na thread de UI por mecanismo único (ex.: `QMetaObject::invokeMethod` com `Qt::QueuedConnection` na bridge).
- Antes de mudar threading, descreva o impacto em lifetime e sincronização (§21).

---

## 14. Formato nativo de projeto

- Formato próprio, **versionado** (`format_version` inteiro no topo).
- O formato persistido **não** espelha structs de memória. Há uma camada de
  serialização explícita (DTOs/schema) entre o modelo e o disco.
- **Migrações explícitas** encadeadas `vN -> vN+1`, cada uma testada com fixture
  da versão antiga. Nunca editar uma migração já publicada.
- Abrir projeto de versão mais nova que a suportada: recusar com mensagem clara (ou modo read-only), nunca corromper.
- Salvamento atômico: escrever em temporário → `fsync` → `rename`. Backup/autosave separado do arquivo principal.
- Deve representar: referências de mídia, timelines, tracks, clips, transições,
  efeitos, keyframes, metadata, e objetos externos não suportados (§16.3).
- Referência de mídia guarda: caminho relativo ao projeto, caminho absoluto e
  uma impressão digital (tamanho + hash parcial + duração/streams) para relink.
- Campos desconhecidos de versões futuras compatíveis são preservados, não descartados.
- Formato concreto (JSON/arquivo único/bundle com SQLite etc.) é decisão de ADR.
  Requisitos: diffável quando possível, robusto a arquivo truncado.

---

## 15. Cache

Projetar a existência de cache desde o início, mesmo que parcial.

Tipos: frames decodificados, thumbnails, waveforms, proxies, frames intermediários renderizados, pipelines de shader.

- **Cache nunca é fonte de verdade.** Apagar todo o cache não perde dados do projeto.
- Toda entrada tem chave derivada das entradas que a produziram (identidade da
  mídia + parâmetros + versão do algoritmo). Mudou a entrada, a chave muda: invalidação por construção.
- Limites de memória/disco configuráveis com política de despejo (LRU ou similar).
- Caches em disco ficam em `$XDG_CACHE_HOME/omamovie/`, nunca ao lado do projeto sem pedido explícito.
- Cache de frames GPU respeita orçamento de VRAM e sincronização de §8.1.

---

## 16. Interoperabilidade e reverse engineering

### 16.1 Três categorias distintas (não misturar)
1. **Formatos de mídia** (containers/codecs) → `oma-media`.
2. **Formatos de interchange** (OTIO, EDL, AAF, FCPXML/XML) → `importers/`.
3. **Projetos proprietários** (Premiere, Resolve, Avid, Vegas, …) → `importers/`.

### 16.2 ProjectIR

```
Projeto externo -> <Formato>Importer -> ProjectIR -> conversor -> Projeto OmaMovie
```

- `ProjectIR` é neutro, independente de qualquer formato externo e do modelo interno.
- Importers produzem **apenas** ProjectIR. Não tocam no modelo interno.
- **Detalhes de formatos externos nunca contaminam o modelo central.** Se o
  modelo interno precisa de um conceito novo, ele é adicionado por mérito próprio, com nome próprio.
- Tempos no ProjectIR usam `RationalTime`, preservando a timebase de origem.

### 16.3 Preservação de dados externos
Nunca descartar silenciosamente. Representar como `UnknownEffect`,
`UnsupportedEffect`, `OpaqueExternalObject`, guardando:
formato de origem, identificador original, payload bruto (quando viável) e
parâmetros parseados parcialmente. O objetivo é permitir round-trip futuro.

### 16.4 Relatório de importação
Todo import gera um relatório estruturado (exibível na UI e na CLI) com, para cada problema:
objeto afetado, motivo, nível de compatibilidade atingido, impacto provável
(ex.: "transição substituída por corte seco; duração preservada").

### 16.5 Níveis de suporte
Nunca escrever "supports Premiere". Declarar por formato **e versão**:

| Nível | Significado |
|---|---|
| 0 | Arquivo reconhecido e versão identificada |
| 1 | Mídia referenciada e clips |
| 2 | Tracks, cortes, transições |
| 3 | Transforms, áudio (gain/fades), keyframes |
| 4 | Efeitos com equivalente mapeado |
| 5 | Round-trip |

Manter uma tabela em `Docs/interop.md` com nível por formato/versão e os testes que comprovam cada nível.

### 16.6 Regras de reverse engineering
- Objetivo: **interoperabilidade**, focado na representação de projetos.
- Começar **read-only**. Escrita só depois de parser compreendido e testado.
- **Proibido** implementar qualquer coisa destinada a contornar DRM,
  licenciamento, criptografia de proteção, autenticação ou controle de acesso.
  Se um formato estiver protegido, documente e pare.
- Ordem de prioridade por acessibilidade: texto → XML → JSON → ZIP/container →
  SQLite → binário estruturado conhecido → binário proprietário → protegido.
  Validar a arquitetura com formatos acessíveis (OTIO, FCPXML, EDL, XMLs de exportação) antes de atacar binários difíceis.
- Documentar descobertas de formato em `Docs/formats/<formato>.md` (estrutura,
  versões observadas, campos conhecidos/desconhecidos, como foi deduzido).

### 16.7 CLI `oma-project`
`inspect` (resumo e nível detectado) · `dump` (árvore estruturada/ProjectIR) ·
`diff` (comparar dois projetos com mudanças pequenas conhecidas, saída
orientada a campos/offsets, para RE) · `validate` (invariantes) · `convert` (via ProjectIR).
A CLI reutiliza as mesmas libs que o app. Nenhuma lógica de parsing existe só na CLI.

---

## 17. Persistência e SQLite

SQLite para: metadata de mídia, índices, caches estruturados, metadata de projeto quando fizer sentido.
- Não usar SQLite "porque está disponível". Justificar no módulo.
- Binários grandes (proxies, frames, waveforms grandes) ficam como **arquivos**; o banco guarda referências.
- Schema versionado com migrações, igual ao formato de projeto.
- Acesso ao banco fora da thread de UI.

---

## 18. Segurança de parsers

Arquivos de mídia e de projeto são **entrada não confiável**, sempre.

- Validar tamanhos, offsets e contagens antes de usar. Aritmética de tamanho com checagem de overflow.
- Limitar alocações (teto por estrutura e total); nunca alocar a partir de um tamanho lido sem limite.
- Arquivos truncados e estruturas inválidas resultam em erro tratado, nunca em crash, UB ou loop infinito.
- Limitar profundidade de recursão em formatos aninhados.
- Parsers XML: desabilitar entidades externas (XXE) e expansão de entidades.
- ZIP: proteger contra zip bombs e path traversal.
- Todo parser de formato externo tem alvo de fuzzing (libFuzzer) e roda em CI com ASan/UBSan.
- Reverse engineering não justifica parser inseguro.

---

## 19. Logging e erros

### Logging
- Logging estruturado via facade em `libs/base` (sem Qt), com sink que pode
  redirecionar para `QLoggingCategory` no app.
- Categorias: `media`, `decode`, `encode`, `gpu`, `compositor`, `timeline`,
  `project`, `importer`, `audio`, `cache`, `playback`, `ui`.
- **Sem `printf`/`std::cout`/`qDebug()` soltos** como observabilidade.
- Logs no caminho quente: não alocar, não formatar strings caras por frame; usar níveis e rate limiting.
- Log em thread RT de áudio é proibido (usar contadores/fila lock-free).

### Erros
- APIs entre subsistemas retornam `std::expected<T, oma::Error>` (ou alias `Result<T>`).
- `oma::Error` tem: código tipado (enum class), categoria, mensagem, contexto (arquivo/stream/objeto).
- **Exceções não atravessam fronteiras de libs.** Se uma dependência lança, capturar na borda e converter.
- Falha silenciosa é proibida. Degradação (ex.: fallback para decode SW) é registrada em log com o motivo.

---

## 20. ADRs

Registrar em `Docs/adr/NNNN-titulo.md` (contexto, decisão, alternativas, consequências).
Obrigatório para decisões sobre: backend gráfico, modelo de threading,
representação temporal, formato de projeto, interop GPU, cache, sistema de import, novas dependências grandes.

Backlog inicial de ADRs:
- 0001 Build system, padrão C++, framework de testes
- 0002 Representação temporal
- 0003 Modelo de threading e job system
- 0004 Abstração de frame GPU e sincronização
- 0005 Integração Qt Quick ↔ compositor Vulkan (device compartilhado ou não)
- 0006 Espaço de cor de trabalho do compositor
- 0007 Formato nativo de projeto
- 0008 ProjectIR e política de preservação de dados externos
- 0009 Estratégia de cache

---

## 21. Regras para agentes de IA

### Antes de modificar arquitetura
1. Entender o módulo afetado e ler sua API pública.
2. Verificar interfaces existentes antes de criar novas.
3. Identificar impacto em **lifetime** e **threading**.
4. Avaliar efeito em **transferências CPU↔GPU**.
5. Verificar testes existentes e o que precisa de teste novo.
6. Explicar alterações estruturais na resposta/PR. Se for decisão de ADR, escrever o ADR.

### Sempre
- Seguir as regras de dependência de §5.2. Violação é bug, mesmo que compile.
- Manter mudanças focadas no pedido. Não reescrever partes grandes por preferência estética.
- Não substituir tecnologias escolhidas sem razão técnica mensurável.
- Ao encontrar problema arquitetural real: documentar (issue/ADR) e propor correção, não contornar em silêncio.
- Não afirmar que algo é "rápido", "em tempo real" ou "zero-copy" sem benchmark ou evidência (ex.: ausência de readback verificada).
- Não inventar números de desempenho como metas.
- Atualizar este arquivo quando uma regra ou comando mudar.

### Nunca
- Chamar FFmpeg fora de `libs/media`; incluir Vulkan/CUDA em timeline/project/IR/importers.
- Colocar lógica pesada em JS/QML.
- Usar `double` como timestamp de referência.
- Criar threads ad hoc; bloquear a thread de UI ou o callback de áudio.
- Descartar dados de projetos externos silenciosamente.
- Implementar contorno de DRM/licença/criptografia/autenticação.
- Adicionar sistema de plugins, IA ou cloud sem decisão explícita.
- Commitar mídia protegida por copyright ou fixtures grandes.

### Pergunte antes de
- Adicionar dependência nova.
- Mudar formato de projeto, representação temporal ou modelo de threading.
- Iniciar suporte a formato proprietário binário.
- Divergir da ordem de desenvolvimento de §24.

---

## 22. Código C++

Preferir: ownership explícito, RAII, tipos fortes (IDs e unidades com tipos
próprios, não `int`/`std::string` crus), const correctness, APIs pequenas,
`enum class`, error handling explícito, nomes claros.

Evitar: globals, singletons indiscriminados, macros desnecessárias, casts
inseguros (`reinterpret_cast`/C-casts sem justificativa), `shared_ptr` como
escolha automática, herança profunda, exceções atravessando subsistemas.

Convenções (ADR-0001; `.clang-format` e `.clang-tidy` na raiz são a referência):
- Tipos e valores de enum `PascalCase`; funções e variáveis `snake_case`; membros privados com sufixo `_`; constantes `kPascalCase`.
- Não usar `describe`, `test`, `it`, `expect`, `bench`, `beforeEach`, `afterEach`, `beforeAll`, `afterAll` como nomes em APIs: são macros do Cest nos testes.
- Headers com `#pragma once`.
- Identificadores, comentários de código e mensagens de log em inglês.
- Comentários explicam o **porquê** (invariantes, sincronização, motivo de uma cópia), não o óbvio.
- Warnings são erros (`WERROR=1` por padrão; conjunto completo no ADR-0001).
- Objetos Vulkan com RAII próprio ou wrapper fino; nenhum `vkDestroy*` manual espalhado.

---

## 23. Performance

Performance é requisito funcional.

No código crítico (decode, compositor, playback, áudio):
- **Medir antes de otimizar.** Profiling com perf, Tracy ou RenderDoc/Nsight/RGP conforme o caso.
- Zero alocações por frame no caminho quente em regime estável; usar pools e buffers reutilizados.
- Evitar cópias grandes; preferir views (`std::span`) e handles.
- Controlar lifetime de recursos GPU explicitamente.
- Evitar sincronização GPU↔CPU desnecessária.
- Instrumentar zonas importantes desde o início (macro/facade de profiling desativável em build release).

### Benchmarks (`tools/bench`)
Cenários iniciais: N streams 1080p60 simultâneos, composição em tempo real, sincronização A/V, latência de interação da timeline, 4K em hardware razoável.

Registrar em JSON: FPS, frame time (média, p95, p99, máx), frames descartados,
CPU, GPU, VRAM, RAM, banda de upload/download quando relevante, **mais
hardware, driver, versão do FFmpeg, codec e caminho de decode usado**.
Resultados sem esse contexto não servem como comparação.

---

## 24. Ordem de desenvolvimento

**Plano detalhado, com marcos, critérios de pronto e releases:
`Docs/implementation-plan.md`.** Ao concluir um marco, atualizar o plano. A
lista abaixo é o resumo da ordem.

Não rígida, mas priorizar:

1. Build system (Make, `base`, testes, sanitizers, clang-format/tidy)
2. Media probing
3. Decode (SW primeiro para corretude, HW em seguida)
4. Abstração de frame GPU
5. Compositor Vulkan
6. Preview
7. Playback clock
8. Sincronização de áudio
9. Modelo da timeline
10. UI da timeline
11. Persistência do projeto
12. Import/export
13. Efeitos
14. Interoperabilidade avançada

**Não começar pela aparência visual final.**

### Primeiro experimento técnico (antes de UI grande)
Protótipo que valida o pipeline:

```
vídeo A + vídeo B + imagem -> GPU compositor -> preview
```

Critérios: abrir múltiplos vídeos; decodificar; manter frames na GPU quando
possível (registrar onde não foi); compor layers via Vulkan com transform,
scale, crop e opacity; apresentar preview; playback sincronizado.
Instrumentar desde o início: frame time, frames descartados, uploads/readbacks.
Pode ser um executável em `tools/` ou `apps/` com janela mínima; o código
reaproveitável vai para as libs, não fica preso no protótipo.

---

## 25. Testes

Obrigatórios para componentes estruturais. Prioridade:
operações da timeline · conversão temporal · serialização · migração de
projeto · importers · ProjectIR · render graph · lifetime de frames · robustez de parsers.

- Cada bug corrigido em componente estrutural ganha teste de regressão.
- Testes de render graph não exigem GPU. Testes de GPU são marcados e podem ser pulados em máquina sem Vulkan.
- Comparação de imagens com tolerância e saída de referência do `CpuBackend`.
- Fixtures pequenas e reproduzíveis em `tests/fixtures/`, com **script gerador**
  (ex.: `ffmpeg -f lavfi -i testsrc2=...`) versionado ao lado. Sem mídia com copyright.
- Para RE: pares de projetos com alterações mínimas conhecidas (um campo por par), quando legalmente possível, documentando o que mudou entre eles.
- TSan para código concorrente; ASan/UBSan para tudo; fuzzing para parsers.

### Escrevendo testes com Cest
- Uma suíte por arquivo `tests/<lib>/test_<tema>.cpp`, exposta como `void run_<tema>_tests()`
  e chamada no `main.cpp` da lib. Nova suíte de lib: `tests/<lib>/module.mk` com `oma_test`
  e um `include` no `Makefile`.
- Incluir `oma_test.hpp` **por último** (depois dos headers do projeto e da STL).
- **Sem vírgulas no nível de topo** dentro de `describe`/`it`: o preprocessador separa os
  argumentos da macro. Evite `std::pair<A, B>`, capturas `[&a, &b]` e `{1, 2}` direto no
  bloco; use `auto`, `[&]`, funções auxiliares ou parênteses.
- `expect()` só na thread do teste; resultados de outras threads passam por atômicos.
- Enums e tipos sem overload no Cest: compare `static_cast<int>(...)` ou um booleano.

---

## 26. Filosofia de implementação

Entre duas abordagens, preferir a que:
preserva separação arquitetural · reduz cópias de memória · facilita testes ·
mantém APIs explícitas · permite profiling · evita dependências ocultas · permite substituir backends.

Mas: não criar abstração sem necessidade real, não generalizar prematuramente,
não construir plugins antes de haver arquitetura estável. Abstração se justifica
por um segundo uso concreto, por uma fronteira de subsistema definida neste documento, ou por testabilidade.

### Pergunta final para toda mudança

> Essa implementação mantém o OmaMovie simples para o usuário sem tornar sua arquitetura simplista?
