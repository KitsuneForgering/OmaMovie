# ADR-0015 — Titles: generated clips rasterized by the app

**Status: Accepted (2026-10-06).** The maintainer chose generated clips over a text property
of video clips and over deferring titles to M8.

## Context

M6 asks for a simple text title in the contextual inspector. Every clip today shows media:
`Clip::media` must name a library item, evaluation yields a media time, and the frame builder
decodes a picture for each layer. Text has no media file, and its pixels need transparency,
which the pipeline does not carry yet: the decoder converts RGB stills to planar GBR and drops
alpha, and the compositors treat every source as opaque. Text rendering needs a font engine;
Qt has one, but the libraries must stay free of Qt (CLAUDE.md §5.2).

## Decision

- **Model (`libs/timeline`).** A clip may carry `title = {text, font, size, color, placement}`
  instead of media: UTF-8 text (at most 1,000 bytes, line breaks allowed), a font family name
  (empty: the UI font), a size as a fraction of the canvas height, straight sRGB RGBA colour,
  and a placement preset (lower third, centre, top). A title clip has no media (`media` is 0),
  sits on a video track, has a constant 1× time map and no audio; `Timeline::validate` checks
  this. Trims, moves, splits, connections (ADR-0014), transforms, opacity and keys apply
  unchanged, because a title is an ordinary clip on a track.
- **Evaluation.** A title clip yields a `VideoLayer` with the title and no media time. The
  render graph stays plain data: the layer's input is a picture like any other.
- **Rasterizing (app).** The frame builder takes the title pictures from a `TitleRasterizer`
  the app provides: Qt's text engine draws the title at the canvas size into an RGBA image,
  converted to a software frame with an alpha plane. Results are cached by (title, canvas
  size) in a small LRU, so playback rasterizes a title once, not per frame. A headless export
  can later supply another rasterizer (e.g. FreeType) behind the same interface.
- **Alpha (`libs/media`, `libs/compositor`).** A software frame may have a fourth plane: planar
  GBRA (`gbrap`) with straight alpha. Both compositors multiply the layer's coverage by it, so
  blend modes, opacity and transitions apply as for any layer. Decoded stills keep dropping
  alpha until a separate change decides that (PNG with transparency).
- **Persistence.** Native format 3: a clip's optional `title` object. Migration 2 → 3 is
  identity; older OmaMovie versions refuse a v3 file instead of dropping titles.
- **UI.** "Add title" (palette and library menu) places a three-second title at the playhead on
  a free lane above the storyline, connected to the storyline clip there; a Title drawer edits
  text, size, colour and placement; the timeline shows the text as the clip's name.

## Alternatives considered

- **Text as a property of a video clip.** Simpler, but a title cannot exist over a gap or a
  black background, cannot be trimmed or moved on its own, and duplicates timing logic.
- **Text drawn by the compositor** (glyph atlas in a shader). Removes the upload but needs a
  font engine in `libs/compositor`; not justified for static titles yet.
- **A title as a generated media item in the library.** Shares the media plumbing, but one
  item would be needed per text variant, and editing a title would mean replacing media.

## Consequences

- Titles reach the GPU as one upload per change through the existing software-frame path,
  cached; this is not the zero-copy path and is fine for static text.
- Alpha in software frames is new and gets compositor tests (CPU reference vs Vulkan).
- Animated titles (per-character effects) are out of scope; keyframed transform and opacity
  animate a title as a whole.
