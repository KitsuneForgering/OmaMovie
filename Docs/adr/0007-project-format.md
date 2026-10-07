# ADR-0007 — Native project format

- **Status:** Accepted (2026-10-04)
- **Milestone:** M7

## Context

A project must survive the application: media references, the sequence (tracks, clips with
every adjustment, transitions, transform keys, markers), LUTs and the canvas. `CLAUDE.md` §14
fixes the requirements: a versioned format with an integer `format_version`, a serialization
layer between the disk and the model, chained migrations, refusal of newer files, atomic and
durable saves, media references by relative path, absolute path and fingerprint, and unknown
fields of compatible newer versions preserved. Project files are untrusted input (§18).

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

## Alternatives

- **SQLite project bundle**: transactional, but not diffable and harder to inspect or repair
  by hand; nothing in a project needs queries. Media metadata caches may still use SQLite
  (ADR-0009).
- **Binary format**: smaller and faster, but opaque; projects are small (kilobytes to a few
  megabytes) and parse in milliseconds as JSON.
- **Qt's QJsonDocument**: Qt is forbidden in the libs (§5.2), and the CLI must read projects
  without Qt.
- **nlohmann-json**: one header for both directions, slower parsing, exceptions by default;
  the maintainer chose simdjson.
- **Replaying commands on load**: keeps one entry point for changes, but cannot express
  states no command creates directly (detached audio flags, dormant transitions) and makes
  loading as slow as editing.

## Consequences

- `libs/project` depends on `timeline` and `base` (plus simdjson); the app and the future
  `oma-project` CLI share it.
- Adding a model field means adding it to the writer, the reader (with a default for older
  files) and the round-trip test; bumping `format_version` only when old readers would lose
  meaning.
- Under clang's sanitizers with libstdc++, simdjson's inline `padded_string` (`new[]` with
  `delete[]`) is reported as an alloc-dealloc mismatch; the fuzz target runs with
  `alloc_dealloc_mismatch=0`. GCC's ASan build of the tests is clean.
- Verified (2026-10-04): byte-identical save → load → save of a project using every saved
  field; refusal of newer versions; preservation of unknown top-level fields; rejection of
  truncated, mistyped, overlapping and deeply nested files; a failed save (read-only folder)
  leaves the previous file intact; 5 000 000 fuzz runs without a failure.
- Version history: 2 adds a clip's `time_map` and `anchor` (ADR-0013, ADR-0014); 3 adds a
  clip's `title` (ADR-0015). Both migrations are identity: older files read unchanged, and
  older OmaMovie versions refuse the newer files instead of dropping what they cannot show.
