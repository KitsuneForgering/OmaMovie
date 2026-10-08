# ADR-0010 — Canvas coordinates: pixels, rescaled when the canvas changes

**Status: Accepted (2026-10-07).** The maintainer chose pixel coordinates with a rescaling
canvas change over normalized coordinates (which would have needed native format 6).

## Context

M8 asks for a changeable canvas aspect ratio (landscape, vertical, square). Layer positions
(`Transform::offset_x/y` and their keys) are pixels of the canvas measured from its centre;
scales are relative and title sizes are already fractions of the canvas height. The canvas size
lived in the app session and the project file, outside the timeline, so a change could not be
undone together with the positions it affects.

## Decision

- Positions stay in canvas pixels; the file format does not change (its `canvas` object already
  holds the size).
- The timeline owns the canvas size (`Timeline::canvas_width/height`, 0 until set). The first
  picture of a new project and an opened project set it without history.
- `edit::set_canvas(width, height)` (even, 16 to 16384 each way) is one undoable edit: it sets the
  size and multiplies every clip's offsets and transform keys by the ratio of the new to the old
  size on each axis, so each layer keeps its place relative to the frame. From an unset canvas
  nothing is rescaled.
- The UI offers 16:9, 9:16, 1:1 and 4:5, keeping the canvas's short side; how a clip fills the
  new frame stays its own Fit setting.

## Alternatives considered

- **Normalized coordinates** (fractions of the canvas): resolution-independent and cleaner for
  a future custom size, but a format migration and a change in the compositor's and the UI's
  geometry for the same visible result today.
- **Keeping pixels without rescaling**: a vertical canvas would leave offset layers off frame.

## Consequences

- A canvas change rescales positions numerically (floating point); repeated changes may drift
  by fractions of a pixel.
- Export uses the canvas size; captions and titles follow it already.
