# ADR-0017 — Captions: a caption track of timed text, SRT/VTT in and out

**Status: Accepted (2026-10-07).** The maintainer chose a caption track of its own (native
format 5) over reusing title clips and over deferring captions.

## Context

M8 asks for a caption track with SRT/VTT import and export. Titles (ADR-0015) are clips: they
are trimmed, moved, connected and composited like pictures, and a long transcript as hundreds of
title clips would mix two different things (styled graphics and accessible text) and make every
caption carry clip machinery it does not need. Subtitle files are untrusted input (CLAUDE.md §18).

## Decision

- **Model (`libs/timeline`).** The timeline holds an ordered list of captions
  `{id, start, duration, text}` in sequence time (on the sequence grid, positive duration, no two
  overlapping), at most 10,000, each text at most 1,000 bytes of UTF-8 with line breaks allowed.
  `validate()` checks it. Captions are not clips: no media, no effects, no transitions.
- **Editing.** `edit::add_caption`, `set_caption` (timing and text) and `remove_caption`, plus
  `replace_captions` for an import, each one undoable command. They compute the new list and
  apply one reversible step that swaps the list, so undo restores it exactly. Captions keep
  their sequence times: storyline ripples do not move them (like unconnected lanes); following
  the storyline would need anchors and is left until a task shows the need.
- **Files (`libs/project`).** `parse_srt`/`parse_vtt` read cues into millisecond times, bounded
  (16 MiB of text, 10,000 cues, 1,000 bytes per cue), tolerant of BOMs, CRLF and VTT headers and
  settings, and refuse malformed timings with the line that failed; overlapping cues are cut at
  the next start. Each has a libFuzzer target. `write_srt`/`write_vtt` produce the files. Times
  snap to the sequence frame grid on import (floor for starts, ceil for ends).
- **Persistence.** Native format 5: an optional `captions` array in the sequence. Migration
  4 → 5 is identity; older builds refuse a v5 file instead of dropping captions.
- **Preview and export.** The viewer shows the caption at the playhead as an overlay (not part of
  the composited picture). Export writes `<movie>.srt` beside the MP4 when the sequence has
  captions; burning captions into the picture is a later option (they would then go through the
  title rasterizer).

## Alternatives considered

- **Titles as captions** (no format change): quick, but captions would inherit clip semantics
  and the caption file round trip would depend on lane conventions.
- **A caption `TrackKind` with caption clips**: reuses track code, but clips need media, and
  track rules (ripple, connections, lanes) would need caption exceptions everywhere.
- **Embedded MP4 subtitle streams (mov_text)**: possible later through the muxer; a sidecar
  `.srt` works with every player and editor.

## Consequences

- Native format 5, with a migration test and a fuzz seed.
- Two new parsers at the untrusted-file boundary, with fuzz targets run by `make fuzz`.
- A caption row in the timeline and a caption editor in the UI.
