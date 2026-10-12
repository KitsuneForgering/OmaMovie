## 5. Threading (ADR-0003)

- UI/main: QML + eventos Qt, despacha comandos p/ Editor ([timeline/editor.hpp](libs/timeline/include/oma/timeline/editor.hpp)).
- Render (Qt): scene graph, `PreviewItem::updatePaintNode` roda no render thread; compositor submete na mesma queue gráfica (recursive lock) — [ADR-0005](../adr/0005-qt-vulkan-interop.md).
- Playback/scheduler: clock, agendamento de frames ([Session](apps/omamovie/src/session.hpp), `onTick()`).
- Decode workers: `VideoScheduler`/threads por stream, com filas limitadas (backpressure). Admission gate controla acesso à queue compartilhada com Qt ([device.hpp](libs/gpu/include/oma/gpu/device.hpp)).
- Audio RT: PipeWire callback ([audio](libs/audio)) — proibido alocar, bloquear, log síncrono, mutex contendido. Usa ring buffers lock-free ([CLAUDE.md §12](CLAUDE.md#12-audio), §13).
- Job pool: `libs/base` job system (threads, cancellation cooperativo, progresso, shutdown ordenado) — usado p/ tarefas background (probe/thumbnails/waveforms/imports/export).

[Return to analysis-implementation.md](analysis-implementation.md)
