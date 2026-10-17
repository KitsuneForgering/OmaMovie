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

[Read the decision](0007-decision.md)

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
