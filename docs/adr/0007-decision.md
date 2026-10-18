## Decision

- **One JSON file** (`*.omamovie`), pretty-printed with one value per line so projects diff
  and merge as text. Top level: `format: "omamovie-project"`, `format_version` (1),
  `canvas`, `media` (the library), `luts`, `sequence` (absent until the first clip).
- **Exact time**: every time is `{"value": n, "timebase": "num/den"}`, rationals as
  `"num/den"` strings, frame rates included (`"30000/1001"`, never 29.97). Doubles (spatial
  and color parameters) are written in their shortest round-trip form.
- **Enumerations by name** (`"fill"`, `"dip_to_black"`), independent of enumerator values.
- **Parsing with simdjson** (DOM API, error codes, no exceptions) inside `libs/project`;
  writing with a small pretty printer (simdjson parses only). The dependency is declared in the
  PKGBUILD (`depends`), chosen by the maintainer over nlohmann-json and a hand-written parser.
- **Model boundary**: `project::Document` (library, LUTs, optional `Timeline`, storyline track,
  canvas) is the serialization layer. Loading rebuilds the timeline with
  `Timeline::restore`, which checks every invariant (`validate`) and continues IDs after the
  largest one saved, so a loaded file cannot hold an inconsistent timeline and new edits
  never reuse an ID. Edits are not replayed; the history starts empty.
- **Versions**: a newer `format_version` is refused with a message asking to update
  (`ErrorCode::Unsupported`), never read in part. Migrations `vN -> vN+1` will chain in
  `from_json` before reading, each tested with a fixture of the old version. Version 2
  (2026-10-05, ADR-0013) adds a clip's optional `time_map`; 1 → 2 is the identity.
- **Unknown fields**: top-level fields this version does not know are kept as raw JSON and
  written back unchanged. Unknown fields inside known objects (a clip, a track) are not kept
  yet; a version 2 that adds some must either nest them under a new top-level field or add
  per-object preservation first.
- **Media references**: absolute path, path relative to the project file, and a fingerprint
  (file size + FNV-1a 64 of the first and last 64 KiB; not cryptographic, so no OpenSSL).
  Loading prefers the relative path when the absolute one is gone (a moved project folder).
  The fingerprint is for relinking; the relink UI is separate work.
- **Atomic save**: temporary file in the same directory (`mkstemp`), write, `fchmod`, `fsync`,
  `rename` over the target, `fsync` of the directory; any failure removes the temporary file
  and leaves the previous project untouched. Autosave writes the same format to a separate
  file (pending).
- **Bounds**: files over 64 MiB, nesting deeper than 32, more than 100 000 items in a list,
  more than 16 curve points or 256 transform keys are rejected. A libFuzzer target
  (`tests/fuzz/fuzz_project.cpp`, `make fuzz`) checks that any input either loads into a
  valid, re-savable document or fails with an error.

[Return to 0007-project-format.md](0007-project-format.md)
