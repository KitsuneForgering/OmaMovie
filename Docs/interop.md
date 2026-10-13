# Interoperability support levels

Support is declared per format **and version**, at the levels of CLAUDE.md §16.5, with the
tests that prove each level. A format with no proven level is not supported. The path is
external project → importer → ProjectIR (ADR-0008) → converter → OmaMovie project.

| Level | Meaning |
|---|---|
| 0 | File recognized and version identified |
| 1 | Referenced media and clips |
| 2 | Tracks, cuts, transitions |
| 3 | Transforms, audio (gain/fades), keyframes |
| 4 | Effects with a mapped equivalent |
| 5 | Round trip |

| Format | Versions | Level | Evidence |
|---|---|---|---|
| OpenTimelineIO JSON (`.otio`) | — | none yet | Pilot planned: own reader on simdjson for a pinned schema subset (ADR-0008) |
| FCPXML / `.fcpxmld` | — | none | Not started |
| CMX 3600 EDL | — | none | Not started |
| Kdenlive (MLT XML) | — | none | Not started |

`libs/project-ir` (2026-10-13) holds the IR, its validation (`tests/project-ir`) and the import
report every importer fills; no importer exists yet.
