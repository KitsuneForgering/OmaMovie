# ADR-0003 — Modelo de threading e job system

- **Estado:** Aceito (2026-10-02)
- **Marco:** M0 (job system); M2–M4 (threads de decode, playback e áudio)

## Contexto

A thread de UI nunca pode bloquear com decode, encode, miniaturas, waveforms, probing,
cache, proxies ou imports (CLAUDE.md §13). Threads criadas ad hoc em componentes tornam
cancelamento, progresso e shutdown imprevisíveis. A pesquisa mostrou que skimming e zoom de
storyboard dependem de cancelar trabalho de miniaturas rapidamente (`Docs/Research/imovie.md` §6).

## Decisão

### Threads do processo
| Thread / pool | Responsabilidade | Regras |
|---|---|---|
| UI (principal) | QML, eventos, despacho de comandos | Nunca bloqueia; recebe resultados por um único mecanismo de fila (M6) |
| Render (Qt) | Scene graph, apresentação do preview | Sincroniza com o compositor por semáforos (ADR-0005) |
| Playback | Relógio, agendamento de frames | Filas limitadas com decode; cancela no seek (M4) |
| Decode | Um pipeline por stream ativo | Fila limitada (backpressure) (M2) |
| Áudio RT | Callback do PipeWire | Sem alocação, sem lock disputado, sem log (M4) |
| **JobPool** | Probe, miniaturas, waveforms, proxies, imports, export | Implementado no M0 |

Decode, playback e áudio são pipelines de tempo real com threads dedicadas; o JobPool é
para trabalho em background sem prazo de frame.

### JobPool (`libs/base/jobs.hpp`, implementado no M0)
- `JobPool(n)` cria `n` workers (`std::jthread`). Nenhum componente cria threads próprias
  para trabalho em background.
- `submit(nome, fn)` retorna um `JobHandle`. A função recebe `JobContext&` e retorna
  `Result<void>`.
- **Cancelamento cooperativo**: `CancellationSource`/`CancellationToken` (flag atômica
  compartilhada). O job verifica `is_cancelled()` em pontos seguros e retorna
  `ErrorCode::Cancelled`. Job pendente cancelado nunca começa.
- **Progresso**: `report_progress(fração)`, lido sem lock pelo `JobHandle`.
- **Estados**: `Pending → Running → Succeeded | Failed | Cancelled`. `wait()` bloqueia e
  retorna o erro do job.
- **Exceções**: jobs não devem lançar; uma exceção que escapa vira `ErrorCode::Internal` e
  o job termina como `Failed` (nenhuma exceção atravessa a fronteira do pool).
- **Shutdown ordenado** (`shutdown()` e destrutor): para de aceitar jobs, cancela os
  pendentes sem executá-los, pede cancelamento dos que estão rodando e faz join dos
  workers. `submit()` após o shutdown retorna um handle já `Cancelled`. Não pode ser
  chamado de dentro de um job.
- Ordem de locks: mutex do pool antes do mutex do job; o worker nunca segura os dois ao
  executar a função do job.

### Logging entre threads
O sink de log roda sob o lock interno do logger: não pode logar, trocar o sink nem segurar
locks que outra thread mantém enquanto loga (inversão detectada pelo TSan durante o M0).

## Alternativas consideradas
- **`std::async`**: sem controle de número de threads, cancelamento ou shutdown.
- **QThreadPool**: acoplaria `libs/base` ao Qt (proibido pelo CLAUDE.md §5.2).
- **Bibliotecas de tasks (TBB, Taskflow)**: mais capazes (work stealing, grafos), mas
  dependência grande sem necessidade atual. Reavaliar se surgirem grafos de jobs.

## Consequências
- A fila do JobPool é FIFO sem prioridade e sem limite. Prioridades (ex.: miniaturas
  visíveis antes das fora da tela) e limite de fila serão adicionados quando o skimming (M6)
  exigir, com medição.
- Entrega de resultados à thread de UI (mecanismo único, ex.: `QMetaObject::invokeMethod`
  com `Qt::QueuedConnection`) é decidida no M6, na camada de app.
- Testes do pool rodam sob TSan na CI (`tests/base/test_jobs.cpp`).
