# ADR-0008 — ProjectIR and preservation of external data

**Status: Proposed (2026-10-13).** The IR shape below is proposed for implementation; the OTIO
parsing route (last section) needs the maintainer's choice before the first importer lands.

## Context

M9 starts interoperability with the path of CLAUDE.md §16.2: external project → importer →
ProjectIR → converter → OmaMovie project. Importers must not touch the internal model, external
details must not leak into it, nothing may be dropped silently (§16.3), and every import yields
a structured report (§16.4). Project files are untrusted input (§18).

## Decision

- **`libs/project-ir` (`oma::ir`), depends on `base` only.** Plain values: a `Timeline` with a
  name, an optional global start, media references `{id, target, available range}` and ordered
  `Track`s (`video`/`audio`) of `Item`s, each a `Clip {name, media id, source range}`, a
  `Gap {duration}` or a `Transition {kind, in offset, out offset}`. Times are `RationalTime`
  in the source's own timebase (§16.2); conversion to the sequence grid happens in the converter.
- **Preservation.** Anything an importer cannot represent becomes an `Opaque {format, original
  id, schema, raw payload, partially parsed fields}` attached to the nearest IR object (or the
  timeline), never discarded. Unsupported effects are opaque objects too.
- **Report.** `ImportReport {format, version, level reached (§16.5), issues}` with each issue
  `{object, reason, level, impact}`; importers and the converter both append to it, and the UI
  and `oma-project` show the same data.
- **Validation.** `ir::validate` checks non-negative durations, positive rates, item ranges inside
  available media ranges when known, and that clip media ids exist; importers run it before
  handing an IR to the converter.

## Open choice: how to read OTIO

1. **Own reader on simdjson** (no new dependency): a pinned subset of the OTIO JSON schemas
   (Timeline.1, Stack.1, Track.1, Clip.2, Gap.1, Transition.1, ExternalReference.1,
   RationalTime.1, TimeRange.1); every other schema becomes an opaque object. Bounded, fuzzed.
2. **OpenTimelineIO C++ library** (new dependency, ASWF): full schema upgrade handling and
   adapters, at the cost of a large dependency and its own error and exception model.

## Consequences

- The converter, not the importers, decides how IR maps onto timeline commands, so one
  importer per format stays small and testable without the app.
- Support levels per format and version are recorded in `Docs/interop.md` with their tests.
