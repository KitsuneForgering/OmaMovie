### 2.5 Audio (`libs/audio`), Playback (`libs/playback`) — Extensibilidade segura mas restritiva

- **Audio RT estrito**: callback PipeWire sem alocação/log síncrono ([CLAUDE.md §12](CLAUDE.md#12-audio)). Extender com filtros/EQ na RT path **deve** respeitar isso (pré-alocar buffers, lock-free). Já há menções a EQ/noise reduction/normalize ([§0](CLAUDE.md#0-repository-status)).
- **Playback clock audio-driven**: [`Session`](apps/omamovie/src/session.*) segue clock áudio (video drop/repeat). Extensão não deve quebrar isso.
- **Audio lanes/waveforms** já mencionados — modelo timeline permite tracks Audio ([TrackKind::Audio](libs/timeline/include/oma/timeline/model.hpp#L185–190)).

Extensibilidade possível, mas exige disciplina RT. Bom.

[Return to analysis-extensibility-modules.md](analysis-extensibility-modules.md)
