# ADR-0019 — Media color interpretation override

**Status: Accepted (2026-10-13).** The maintainer chose a per-media override over closing the
M9 color item or researching HDR first.

## Context

Decoding converts YUV to RGB with the matrix and range the stream declares (ADR-0006,
CLAUDE.md §7.4). Untagged or mistagged files are common: the 2026-10-07 export check found an
untagged HD file that ffmpeg had encoded with BT.601 while OmaMovie reads untagged HD as
BT.709, a visible hue shift with no way for the user to correct it.

## Decision

- **Model (`libs/timeline`).** `MediaInfo` gains `color_matrix` (Auto, BT.601, BT.709,
  BT.2020) and `color_range` (Auto, Limited, Full). Auto keeps what the stream declares (and
  the existing untagged fallbacks). They are the media's interpretation, not a grade: every
  clip of that media follows them. Own enums, so the timeline does not depend on `media`.
- **Editing.** `edit::set_media_color(media, matrix, range)`: one undoable command.
- **Rendering.** `build_viewer_frame`, which every viewer, playback, scope and export frame goes
  through, rewrites each decoded picture's matrix and range before compositing; the filter
  previews do the same. Primaries and transfer are unchanged (HDR stays degraded, ADR-0006).
- **Persistence.** Native format 7: optional `color_matrix`/`color_range` strings on media;
  absent means Auto. Migration 6 → 7 is identity; older builds refuse a v7 file.
- **UI.** Two choices in the Info drawer for the selected clip's media.

## Alternatives

- Per clip: one file read two ways in one project is not a real case, and a per-media setting
  cannot drift between clips of the same take.
- A grade-time "601 ↔ 709" look: it would act after the YUV→RGB step and could not undo a wrong
  range clip (crushed blacks are lost), so the fix belongs at the conversion.

## Consequences

- The library thumbnail still shows the stream's own interpretation until it is regenerated.
- The setting must be checked against an independent oracle (libswscale with the same matrix
  and range), like the existing tagged-format checks.
