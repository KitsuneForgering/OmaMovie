# Format extension boundaries

This note records the media-format boundary used by M7 and the separate project-interchange work planned for M9.

## Media files (M7)

`oma::media::MediaImporter` is the editor-facing interface for inspecting an input file. `ExportFormatChecker` is a separate interface for checking a proposed output format. The current `FfmpegFormatBackend` implements both. Input inspection calls `probe()`, then opens the chosen streams in software and decodes their first frame or audio buffer before accepting the file. This catches missing decoders and early damage, not corruption later in a source. `Session` owns an importer for the lifetime of its import worker, so another in-process implementation can be supplied at construction without changing QML or timeline code. There is no dynamic plugin loader.

`ExportFormat` is a request DTO: FFmpeg muxer name, video encoder name, and optional audio encoder name. `check_output()` rejects missing names, absent muxers or encoders, wrong stream kinds, and codec/container pairs that FFmpeg reports incompatible. FFmpeg can also report that compatibility is unknown; the check rejects that case conservatively. Passing this check does **not** prove that an encoder can open with a chosen pixel format, resolution, profile, hardware device, or rate control, or that a complete movie can be written. The writer implements four tested profiles: MP4/H.264/AAC, Matroska/H.264/AAC, Matroska/HEVC/AAC and WebM/VP9/Vorbis. The export dialog lists only profiles whose named encoders are available; the writer still checks its actual settings when opened. It writes a temporary file and replaces the destination only after completion.

Do not add a class per extension or codec: one FFmpeg backend covers its registered demuxers, muxers, and encoders. New backend implementations need a real difference in format handling or ownership. Keep UI file filters advisory; the demuxer and probe result decide what can be imported.

## Project interchange (M9)

Project files have a different boundary from media containers. M7 first needs a versioned native project DTO and durable save/load, with the decisions recorded in ADR-0007. M9 then introduces `ProjectIR` and format importers that map external timelines into it, preserving rational times and reporting unsupported objects. An exporter for an interchange format should be added only with a tested round trip at a declared support level and version; a virtual `ProjectExporter` without one working format has no useful contract to validate.

OpenTimelineIO is the planned first pilot because its schema and adapter model are public. Its [adapter documentation](https://opentimelineio.readthedocs.io/en/latest/tutorials/adapters.html) separates native OTIO JSON from additional format plugins, and its [feature matrix](https://opentimelineio.readthedocs.io/en/v0.14/tutorials/feature-matrix.html) illustrates that mappings differ by feature and format. Those documents guide test design; they do not establish fidelity for OmaMovie's future importer.
