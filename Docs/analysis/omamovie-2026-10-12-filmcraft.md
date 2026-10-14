## FilmCraft as an external engineering comparator

Source scope, accessed 2026-10-12: FilmCraft
[workspace manifest](https://github.com/storytold/filmcraft/blob/main/Cargo.toml),
[repository](https://github.com/storytold/filmcraft), and its
[gap inventory](https://github.com/storytold/filmcraft/blob/main/docs/gaps.md).
FilmCraft `main` moves rapidly; these sources are not a pinned runtime build.
Its README/gaps are project-authored claims. No matched task, binary, benchmark or
test suite was run here, so comparative correctness and speed are unranked.

| Lens | OmaMovie observed here | FilmCraft source evidence and limit |
|---|---|---|
| Project/time model | Versioned JSON v6; rational sequence/media time and exact grid; typed tracks/clips | Manifest has `time`, `project`, `format` crates; README claims integer ticks, not independently exercised here |
| Commands/undo | Typed `edit::*` plans/primitives, atomic `Editor::execute`, redo reuses plan | Manifest has `edit`/`engine`; README claims shared command registry; exact rollback not audited here |
| UI/core/automation | Core avoids Qt; Session/QML actions use stable discrete IDs; no parameterized public schema | Manifest has `ui-egui`, `automation`, CLI; README claims one command path; integration not run here |
| Media/render/audio | FFmpeg handles codec/container decode/encode; OmaMovie owns timestamp, frame interop, compositor and mixer | Manifest includes own codec/container and GPU/audio crates, a much larger owned critical surface; fidelity claims need corpus verification |
| Memory/lifetime | C++ and Vulkan/FFmpeg/Qt FFI need explicit ownership; ASan/TSan cover libraries | Rust reduces ordinary memory-unsafe surface; FFI/GPU and logic/timing errors remain; no runtime comparison |
| Tests/oracles | Explicit rational/command tests, project/subtitle/LUT fuzz, CPU/GPU vectors, ffprobe outputs; property/QML workflow gaps | Manifest includes `testkit`/`golden` and `proptest`; gaps document incomplete real-media corpus; test quality not inferred from presence |
| PR process | OmaMovie workflow runs matrix/lint/fuzz, but merge checks not enforced and app omitted from sanitizers | FilmCraft gap inventory says PR test CI absent; verify pinned workflow before stronger comparative claim |
| Product breadth | One recording/phone-video flow partly validated; no OTIO, multicam, full mixer/HDR | README describes broader editing/delivery breadth; gaps report Linux startup/playback and media holes; self-reported breadth is not readiness |

FilmCraft's own format/codec implementations make it responsible for bitstream,
container and hardware-interaction correctness that OmaMovie delegates to FFmpeg.
OmaMovie still owns policies and seams where timestamp, source identity, zero-copy
claims, output publication and UI state can fail. Fewer owned lines do not make
either design automatically safer. The useful transfer is a reproducible,
externally decoded task and selective property/oracle checks, not a codec rewrite,
Premiere feature checklist or a second command system.

OmaMovie's run of FilmCraft's benchmark scenarios on 2026-10-14:
[filmcraft-benchmarks](omamovie-2026-10-14-filmcraft-benchmarks.md).

[← Assessment](omamovie-2026-10-12.md)
