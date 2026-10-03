# Plano de implementação — OmaMovie

> Criado em 2026-10-02 com base em `Docs/Research/`. Regras de engenharia e
> invariantes estão no `CLAUDE.md`; este documento define **o que construir, em
> que ordem e como saber que terminou**.
>
> Atualizar este arquivo ao concluir cada marco (marcar os itens, registrar
> desvios). Divergências estruturais exigem ADR (`CLAUDE.md` §20).

---

## 1. Objetivo

Construir o editor de vídeo nativo do Omarchy para **todos os tipos de
criadores de conteúdo**: abre direto o vídeo do celular e as gravações de tela
do Omarchy, toca em tempo real numa GPU integrada, funciona offline, sem conta e
sem paywall, com a simplicidade do iMovie/CapCut sobre um motor que cresce até o nível do Resolve.

Bases da pesquisa:

| Achado | Documento | Efeito no plano |
|---|---|---|
| Resolve no Linux não abre H.264/AAC na versão gratuita e só suporta NVIDIA oficialmente | `davinci-resolve.md` §11 | Decode de formatos de consumidor em Intel/AMD/NVIDIA é prioridade da v0.1 |
| Kdenlive é o editor padrão do Omarchy | `omarchy-integration.md` §1 | Meta: virar o editor padrão do Omarchy |
| Gravações do Omarchy (ALT+PRINT): H.264/HEVC/AV1, 60 fps CFR, AAC, em `~/Videos` | `omarchy-integration.md` §5 | Fonte "Gravações" já na v0.1 |
| Estabilidade e perda de trabalho são a principal reclamação contra Premiere, CapCut e Shotcut | `premiere-pro.md` §10, `capcut.md` §3 | Salvamento atômico, autosave e fuzzing desde o primeiro formato |
| Inspector contextual, presets por intenção, timeline magnética como política | `imovie.md`, `capcut.md` | Desenho da UI v0.1/v0.2 |
| Device Vulkan único compartilhado por FFmpeg, compositor e Qt | `vulkan.md`, `hardware-strategy.md` | Spikes antes do código definitivo |
| NVDEC/NVENC sem CUDA toolkit; kernels CUDA só com ganho medido | `cuda.md` | CUDA em runtime, build opcional, fase tardia |
| OpenCL removido | `opencl.md` | Compute = Vulkan + CPU (+ CUDA depois) |

---

## 2. Releases

Cada release é utilizável por um público real; nenhuma é demo técnica.

| Release | Nome | Para quem | Conteúdo resumido |
|---|---|---|---|
| **v0.1** | Primeiro corte | Quem grava e edita vídeos simples (vlog, tutorial, gravação de tela) | Importar (inclui gravações do Omarchy), timeline multitrack, split/trim/ripple, transform/crop/opacidade, ganho/fades de áudio, títulos simples, undo/redo, J/K/L, salvar projeto, export H.264/HEVC/AV1 por hardware, tema do Omarchy |
| **v0.2** | Criador | Vídeo curto/vertical, YouTube, redes sociais | Legendas (track própria, SRT/VTT), proporção do canvas (vertical/horizontal), velocidade e speed ramps com presets, keyframes, transições, presets (PiP, split screen, cutaway), marcadores, proxies, render em background, zoom storyboard↔timeline |
| **v0.3** | Interop e cor | Quem vem do Resolve/Kdenlive/FCP | Import OTIO, FCPXML, EDL, Kdenlive; relatório de importação com níveis; presets de gerenciamento de cor, LUTs `.cube`, scopes; HDR |
| **Futuro** | — | — | Import `.prproj`, backend CUDA para efeitos, beat sync, remoção de silêncio, widget no shell do Omarchy, propostas upstream ao Omarchy |

Não objetivos (`CLAUDE.md` §2) continuam valendo em todas as releases.

---

## 3. Visão geral dos marcos

```
M0 Fundação ──┬──────────────────────────────────────────────┐
              │                                              │
M1 Spikes de hardware (S1–S6, em paralelo ao fim do M0)       │
              │                                              │
M2 libs/gpu + libs/media ── M3 Compositor ── M4 Playback/áudio│
                                   │                         │
M5 Modelo da timeline (pode começar após M0, sem GPU) ───────┘
                                   │
                    M6 UI v0.1 ── M7 Projeto + export ──► v0.1
                                                          │
                                   M8 Criador ──────────► v0.2
                                   M9 Interop e cor ────► v0.3
```

O M5 depende só de `libs/base` e pode avançar em paralelo ao M2–M4.

---

## 4. Marcos

Legenda: **Entregas** = o que existe no fim. **Pronto quando** = critério
verificável. **Pesquisa** = de onde veio a decisão.

### M0 — Fundação ✅ (2026-10-02)

**Entregas**
- [x] Pacotes de build: `make gcc clang ffmpeg` (M0). Para o M1 faltam instalar `vulkan-headers vulkan-tools libva-utils`.
- [x] `Makefile` não recursivo (GNU Make, ADR-0001): `BUILD=debug|release|asan|tsan`, `module.mk` por lib e por suíte de testes.
- [x] Flags completas de warning com `-Werror` nas libs; `.clang-format`, `.clang-tidy` (warnings são erros), `.editorconfig`, `.gitignore`.
- [x] **Grafo de dependências do §5.2 verificado pelo Make**: dependência proibida ou lib não registrada é erro antes de compilar. Só `libs/base` existe; as outras libs entram nos marcos delas.
- [x] `libs/base`:
  - [x] `Rational`, `RationalTime`, `TimeRange`, `FrameRate`, `SampleRate`; `rescale` em 128 bits com detecção de overflow e arredondamento explícito; comparação exata entre timebases.
  - [x] `oma::Error` + `Result<T>` (`std::expected`).
  - [x] Facade de logging com as categorias de §19, sem Qt.
  - [x] Job system: pool de threads, cancelamento cooperativo, progresso, shutdown ordenado.
- [x] Testes com **Cest** (vendorizado em `third_party/cest`, fixado por commit), `make test`, filtro e JUnit.
- [x] CI (GitHub Actions, container Arch): g++ e clang++ × debug/asan/tsan/release, mais formato e clang-tidy. **Ainda não rodou no GitHub** (repositório sem push).
- [x] Script gerador de fixtures (`tests/fixtures/generate.sh`): H.264 CFR/NTSC/VFR, HEVC 10 bits, AV1+Opus, rotação, truncado, WAV 44.1 kHz, PNG.
- [x] ADR-0001 (Make, C++23, Cest, convenções, licença), ADR-0002 (tempo e `TimeMap` para speed ramps), ADR-0003 (threading e job system).

**Pronto quando**: `make test` passa local e na CI; testes de tempo cobrem
23.976/29.97/59.94, 44.1/48/96 kHz, horas em timebase de 90 kHz, arredondamento e
overflow; o `CLAUDE.md` §3 reflete os comandos reais.

**Resultado local**: 45 testes (109 asserções) passam em `debug`, `release`, `asan` e
`tsan` com GCC 16, e em `debug` com Clang 22; `make tidy` e `make format-check` limpos.
O TSan encontrou uma inversão de ordem de locks no teste de logging, corrigida e
documentada no contrato do sink.

**Pesquisa**: `CLAUDE.md` §6, `capcut.md` §4.2 (curvas de velocidade).

---

### M1 — Spikes de hardware

Código em `tools/spikes/`, descartável; o que provar valor migra para as libs no M2/M3.
Resultados em `Docs/spikes/Sn-<nome>.md` com números, versões de driver e conclusão.

| Spike | Pergunta | Pronto quando | Alimenta |
|---|---|---|---|
| **S1** Inventário | Que codecs/perfis a Iris Xe decodifica e codifica via Vulkan Video, VA-API e QSV? | Tabela codec × API × bit depth | ADR-0004 |
| **S2** FFmpeg no device do OmaMovie | O FFmpeg 9 aceita um `VkDevice` criado com Vulkan-Hpp e entrega `AVVkFrame` sem readback? Como funciona o lock de fila? | Decode H.264/HEVC/AV1 em `AVVkFrame`, zero cópia para CPU confirmada | ADR-0004 |
| **S3** Vulkan Video vs. VA-API | Qual caminho é melhor por codec na Iris Xe? | Frame time, CPU e potência por codec; política de seleção escrita | ADR-0004 |
| **S4** Qt Quick no mesmo device | `QQuickGraphicsDevice::fromDeviceObjects` + `QSGVulkanTexture::fromNative` exibem a imagem do compositor sem cópia? | Preview a 60 fps, sem deadlock entre Qt, FFmpeg e compositor | ADR-0005 |
| **S5** Composição mínima | 2 vídeos + 1 imagem com transform/crop/opacity e YUV→RGB | Frame time em 1080p60 medido e registrado | M3 |
| **S6** libplacebo | Opera sobre imagens/device do OmaMovie sem cópia? Qualidade e custo? | Decisão "usar libplacebo para cor/escala/LUT ou shaders próprios" | ADR-0006 |

S7 (caminho NVIDIA) e S8 (AMD) exigem outro hardware; ficam agendados para quando houver máquina (§7).

**Pronto quando**: ADR-0004, 0005 e 0006 escritos com dados dos spikes.

**Pesquisa**: `hardware-strategy.md` §5, `vulkan.md` §5–7.

---

### M2 — `libs/gpu` e `libs/media`

**`libs/gpu`**
- [ ] Criação de `VkInstance`/`VkDevice` com Vulkan-Hpp (`vk::raii`, `VULKAN_HPP_NO_EXCEPTIONS` + `VULKAN_HPP_RAII_NO_EXCEPTIONS` → `std::expected`), include confinado a `.cpp` + PCH.
- [ ] Lista de extensões: interop (DMA-BUF, modificadores DRM, external memory/semaphore fd), vídeo (decode/encode), as exigidas pelo Qt.
- [ ] Lock de fila único, compartilhado com FFmpeg e Qt.
- [ ] VMA para alocações próprias; caminho separado para memória importada.
- [ ] Wrappers de timeline semaphore; destruição ordenada (consumidores → pools → device).
- [ ] Tabela de capacidades + seleção de device (notebooks híbridos: um device primário; não acordar a dGPU à toa).

**`libs/media`**
- [ ] Probe → estrutura própria (streams, codec, timebase, duração, cor, rotação, layout de áudio).
- [ ] Demux, decode software (referência de corretude).
- [ ] Decode por hardware com política por codec/driver do S3 (Vulkan Video, VA-API mapeado, NVDEC na NVIDIA), fallback software registrado em log.
- [ ] `Frame` embrulhando `AVVkFrame` (contrato de timeline semaphore), pool de frames, sem alocação por frame em regime estável.
- [ ] Seek preciso por PTS (VFR), conversão de `AV_NOPTS_VALUE` para `std::optional`.
- [ ] Decode de áudio + resample para float32 planar.
- [ ] Intermediários em compute shader (FFv1/ProRes) verificados no FFmpeg 9.

**Testes**: fixtures CFR/VFR, 8/10 bits, H.264/HEVC/AV1, rotação, AAC/Opus,
arquivos truncados; **gravação real do Omarchy** (gerada localmente); alvo de fuzzing para o probe.

**Pronto quando**: decode em tempo real de 1080p60 H.264/HEVC/AV1 na Iris Xe
**sem readback**, medido com `oma-bench`; testes de seek passam em VFR.

**Pesquisa**: `vulkan.md`, `cuda.md` §2–3, `omarchy-integration.md` §5.

---

### M3 — Compositor e render graph

- [ ] Render graph como dados puros (testável sem GPU).
- [ ] Compositor Vulkan: YUV→RGB lendo matrix/range/primaries/transfer; transform, crop, opacidade, blend modes básicos; pilha de layers.
- [ ] Espaço de trabalho conforme ADR-0006 (float16 linear proposto); saída RGBA para o preview.
- [ ] Se o S6 aprovar: libplacebo como estágio de cor/escala.
- [ ] Shaders GLSL/Slang → SPIR-V no build; pipeline cache persistido.
- [ ] `ComputeBackend` com `VulkanComputeBackend` e `CpuBackend` (referência); efeitos declaram capacidade GPU; trechos sem tempo real são reportados.

**Pronto quando**: o cenário do S5 roda com o código das libs; comparação
GPU × CPU dentro da tolerância; frame time de 1080p60 com 3 layers medido e registrado.

**Pesquisa**: `CLAUDE.md` §9, `premiere-pro.md` §2 (barra de render), `davinci-resolve.md` §5 (grafo por dentro, pilha por fora).

---

### M4 — Playback e áudio

- [ ] Saída PipeWire; callback de tempo real sem alocação nem lock disputado.
- [ ] Relógio mestre = áudio; vídeo descarta/repete frames.
- [ ] Agendador de playback: decode adiantado com filas limitadas, cancelamento no seek.
- [ ] J/K/L (velocidades múltiplas), frame step.
- [ ] Janela mínima Qt Quick com o preview do S4 (ainda sem UI final).

**Pronto quando**: drift A/V **medido** em 10 minutos e meta definida a
partir da medição; seek e J/K/L sem travar a UI; TSan limpo.

**Pesquisa**: `CLAUDE.md` §12–13.

---

### M5 — Modelo da timeline (paralelo ao M2–M4)

- [ ] Tracks (vídeo, áudio; tipo legenda já previsto no modelo), clips por ID de mídia estável.
- [ ] Comandos com transação única no histórico: insert, overwrite, split, trim, ripple, roll, slip, slide, delete, ripple delete.
- [ ] Undo/redo; `validate()` de invariantes.
- [ ] Magnetismo como **política da UI** sobre comandos genéricos.
- [ ] Marcadores no modelo.
- [ ] Mapeamento de tempo do clip conforme ADR-0002 (velocidade constante agora, estrutura pronta para curva).
- [ ] Geração do render graph para um tempo `t`.
- [ ] Comandos serializáveis e sem dependência de UI (base futura para scripting/CLI).

**Pronto quando**: todas as operações testadas, incluindo combinações e
undo/redo de transações; snapshots de render graph para estados conhecidos.

**Pesquisa**: `premiere-pro.md` §3, `imovie.md` §5, `davinci-resolve.md` §10.

---

### M6 — UI v0.1

Design de referência: `Docs/ui-design.md`.

- [ ] Shell Qt Quick: MediaPanel, PreviewPanel, TimelineView, Inspector, TransportControls.
- [ ] MediaPanel com fonte **"Gravações"** (observa `$XDG_VIDEOS_DIR`); miniaturas e waveforms via job system + cache invalidável.
- [ ] TimelineView: tracks, arrastar, aparar, split, snapping, zoom **centrado no mouse**, barra de visão geral (minimapa).
- [ ] Inspector contextual: vídeo (transform, crop, opacidade), áudio (ganho, fades), texto (título simples); indicador de ajustes ativos no clip.
- [ ] Manipulação direta no preview (posição, escala, crop) emitindo comandos.
- [ ] Sistema central de ações (id, nome, atalho), independente do dispositivo de entrada; edição completa por teclado.
- [ ] Módulo de plataforma Omarchy (`apps/omamovie/src/platform/omarchy/`): tema via `omarchy-theme-color --all` + watcher em `~/.local/state/omarchy/current/` + fallback; fonte do Omarchy; app_id `omamovie`.
- [ ] `omamovie.desktop`, ícone, documentação da regra de opacidade do Hyprland.
- [ ] Tela de Projetos (recentes + gravações recentes com "Editar") e regras de abertura por origem; instância única (`ui-design.md` §3.1).
- [ ] Fonte do Omarchy em toda a UI, com troca ao vivo (`ui-design.md` §10.1).
- [ ] Conjunto inicial de ícones SVG próprios para a v0.1 (`ui-design.md` §10.2).
- [ ] Atalhos na convenção iMovie/Final Cut, conferidos com a documentação oficial (`ui-design.md` §8.1).

**Pronto quando**: editar um vlog curto do início ao fim, inclusive só pelo
teclado; a thread de UI nunca bloqueia (medido); a troca de tema do Omarchy aplica ao vivo sem tocar no preview.

**Pesquisa**: `imovie.md` §3–6, `movie-maker.md` §5 (layout do Clipchamp), `other-editors.md` §3 (zoom no mouse), `omarchy-integration.md` §3–4.

---

### M7 — Projeto, export e release v0.1

- [ ] ADR-0007 (formato de projeto) e ADR-0009 (cache).
- [ ] Serialização versionada (DTOs separados do modelo), migrações encadeadas, salvamento atômico, autosave separado, relink por impressão digital.
- [ ] Índice de mídia em SQLite, se justificado no ADR.
- [ ] Export: render graph → encoder (Vulkan/VA-API em Intel/AMD, NVENC na NVIDIA, software como fallback) + mux, como job em background com progresso e notificação (D-Bus).
- [ ] `oma-project inspect | validate | dump` para o formato nativo.
- [ ] Empacotamento: PKGBUILD, `omastore.toml`, release no GitHub.

**Pronto quando**: ida e volta save/load sem diferença; fuzzing do loader sem
crash; arquivo exportado validado com `ffprobe` e A/V sincronizado; instalação
pelo PKGBUILD numa máquina Omarchy limpa.

**→ Release v0.1**

---

### M8 — v0.2 Criador

- [ ] Track de legendas, import/export SRT/VTT.
- [ ] Proporção do canvas alterável (ADR: coordenadas normalizadas vs. pixels).
- [ ] Velocidade constante + speed ramps com presets nomeados (curva do ADR-0002).
- [ ] Keyframes com interpolação em todos os parâmetros animáveis; dopesheet simples.
- [ ] Transições (dissolve, dip, wipe).
- [ ] Presets por intenção (PiP, split screen, cutaway) gerando layers editáveis.
- [ ] Marcadores na UI.
- [ ] Proxies (intermediários decodificados em compute) e alternância proxy/original.
- [ ] Render em background preenchendo o cache onde o tempo real não é possível.
- [ ] Zoom semântico storyboard ↔ timeline.

**Pesquisa**: `capcut.md` §4.2, `imovie.md` §5, `movie-maker.md` §4, `other-editors.md` §1.

### M9 — v0.3 Interop e cor

- [ ] `libs/project-ir` + framework de importers + relatório de importação + `Docs/interop.md` com níveis por formato.
- [ ] Import OTIO (primeiro; o Resolve exporta nativamente), FCPXML, EDL, Kdenlive (MLT XML).
- [ ] `oma-project diff | convert`.
- [ ] Presets de gerenciamento de cor, LUTs `.cube`, scopes; HDR.

**Pesquisa**: `davinci-resolve.md` §8, `premiere-pro.md` §5, `CLAUDE.md` §16.

---

## 5. Decisões e ADRs

| Decisão | Quando | ADR | Proposta inicial |
|---|---|---|---|
| Build, C++23, framework de testes, licença | M0 | 0001 | **Aceito**: GNU Make, C++23, Cest |
| Representação temporal + curva de velocidade | M0 | 0002 | **Aceito**: `RationalTime` + `TimeMap` monotônico por partes |
| Threading e job system | M0 | 0003 | **Aceito**: threads de pipeline + `JobPool` |
| Frame GPU, sincronização, política de decode | M1 | 0004 | `AVVkFrame` embrulhado; seleção por codec/driver |
| Qt Quick ↔ compositor | M1 | 0005 | Device único via `fromDeviceObjects` |
| Espaço de cor + libplacebo | M1 | 0006 | Float16 linear; libplacebo conforme S6 |
| Formato de projeto | M7 | 0007 | Arquivo como fonte de verdade, diffável |
| ProjectIR e preservação | M9 | 0008 | `CLAUDE.md` §16 |
| Cache | M7 | 0009 | Chave derivada das entradas, `$XDG_CACHE_HOME/omamovie` |
| Coordenadas do canvas | M8 | 0010 | A decidir antes de proporção alterável |

Pendentes de aprovação para entrar no `CLAUDE.md` (`hardware-strategy.md` §4):
Vulkan-Hpp sem exceções, device único, política de CUDA, notebooks híbridos, integração Omarchy.
Proposta: confirmar no M0 junto com os ADRs 0001–0003.

---

## 6. Testes e qualidade por marco

| Marco | Obrigatório |
|---|---|
| M0 | Testes de tempo; sanitizers na CI |
| M2 | Fixtures de mídia; fuzzing do probe; teste de lifetime de frames |
| M3 | Render graph sem GPU; comparação GPU × CPU |
| M4 | TSan; drift A/V medido |
| M5 | Todas as operações e transações de undo |
| M6 | UI thread nunca bloqueada; teste de troca de tema |
| M7 | Ida e volta save/load; migrações; fuzzing do loader; export validado |
| M9 | Fuzzing de cada importer; fixtures pequenas por nível de compatibilidade |

Benchmarks (`tools/bench`) registram sempre hardware, driver, versão do FFmpeg, codec e caminho de decode (`CLAUDE.md` §23).

---

## 7. Matriz de hardware

| Hardware | Disponível | Usado em |
|---|---|---|
| Intel Iris Xe (TigerLake) | **Sim** (máquina de desenvolvimento) | M1–M7, alvo "hardware modesto" |
| AMD (RADV) | Não | S8; necessário antes da v0.1 |
| NVIDIA (nvidia-open) | Não | S7, NVDEC/NVENC, interop; necessário antes da v0.1 |
| Notebook híbrido | Não | Seleção de device |
| CI (container, sem GPU) | Sim | Caminhos de software |

**Bloqueio conhecido**: a v0.1 promete Intel, AMD e NVIDIA. Sem máquinas AMD e
NVIDIA (ou contribuidores com esse hardware), a v0.1 precisa declarar só Intel como validado.

---

## 8. Riscos

| Risco | Marco | Mitigação |
|---|---|---|
| Sincronização de fila entre Qt, FFmpeg e compositor | M1 (S4) | Spike antes da UI; lock único; ADR-0005 |
| Diferenças de driver em DMA-BUF/modificadores | M1–M2 | Usar o mapeamento do FFmpeg; fallback GPU→GPU registrado |
| Falta de hardware AMD/NVIDIA | M7 | Declarar o que foi validado; buscar testadores cedo |
| CUDA não aceita wait-before-signal | Futuro | Ordem garantida pelo agendador |
| Compilação lenta do Vulkan-Hpp | M2 | Include confinado + PCH |
| Escopo crescendo antes da v0.1 | Todos | Nada da v0.2 entra antes da v0.1 sair |
| Qt 6 sem CMake (moc, rcc, registro de tipos QML) | M4–M6 | Regras de Make com `pkg-config Qt6*`; validar com uma janela mínima no M4 antes da UI (ADR-0001) |

---

## 9. Próximo passo

Começar o **M0**: instalar `vulkan-headers vulkan-tools libva-utils`, criar o
build system e `libs/base`, e escrever os ADRs 0001–0003. O S1 (inventário de
hardware) pode rodar no mesmo dia, porque só precisa de `vulkaninfo` e `vainfo`.
