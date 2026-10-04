# ADR-0012 — Color grading: CDL, curves and 3D LUTs

- **Status:** Accepted (2026-10-04)
- **Milestone:** ahead of M8 (maintainer choice, see the implementation plan)
- **Research:** [GPU effects](../Research/gpu-effects.md), with local measurements and numerics

## Context

The Color drawer had exposure, contrast, saturation and temperature in linear light. Grading
tools (color wheels, curves, LUTs) are defined for gamma-encoded images: a LUT made for Rec.709
video expects code values, and curves are drawn over them. The compositor's working space is
linear BT.709 (ADR-0006 proposal) and both compositors must keep producing the same pixels, the
CPU one being the reference (CLAUDE.md §9.3). The timeline may not depend on the compositor
(§5.2) and clips reference external files by stable ID (§10).

## Decision

- **Order and space.** Each layer may carry a grade, applied after its look (color
  adjustments, filter, vignette): ASC CDL, then curves, then a 3D LUT. The grading space is
  BT.709 primaries **re-encoded with the inverse of the layer's own transfer function**
  (BT.1886, sRGB or linear) and clamped to [0, 1], so the tools see the file's code values; the
  result is decoded back to linear light. The stage runs only when the clip has a grade, so
  ungraded clips keep values above 1.
- **Wheels are stored as ASC CDL v1.2** (slope, offset, power per channel, saturation; clamps
  as in the ASC style). The UI shows lift/gamma/gain wheels, which map onto it exactly:
  slope = gain (1 − lift), offset = gain · lift, power = gamma (checked to 6e-15). Each wheel's
  tint spreads over the channels at 0°, 120° and 240° and inverts exactly, so the drawer reads
  the wheels back from the CDL.
- **Curves** are monotone cubic (Fritsch–Carlson) through 2–16 points per curve, flat beyond
  the ends, evaluated directly in the shader from the points and their tangents (computed on
  the CPU). Each channel goes through master, then its own curve.
- **3D LUTs** are interpolated **tetrahedrally** with `texelFetch` from an RGBA32F 3D image, the
  same arithmetic as the CPU reference; `lut_amount` mixes before and after. LUT images are
  uploaded once and cached by table (least recently used out past eight).
- **`.cube` parsing** (`compositor::parse_cube`): `LUT_3D_SIZE` 2–65, red fastest,
  `DOMAIN_MIN/MAX` (Adobe) and `LUT_3D_INPUT_RANGE` (Resolve), `TITLE`, comments; 1D LUTs are
  refused; at most 16 MiB of text. It has a libFuzzer target (`make fuzz`), run in CI.
- **Model.** `timeline::ColorGrade` mirrors the compositor's grade with plain types; clips
  reference a LUT by `LutId`, registered in the timeline like media (`Editor::add_lut`), so
  `validate()` rejects unknown references. The app holds the tables (`LutTables`, shared and
  immutable) and maps one onto the other when it builds the render graph.

## Alternatives

- **Trilinear interpolation, or the sampler's filtering**: trilinear tints greys (2.9e-3 on a
  33³ LUT, above a 10-bit step) where tetrahedral keeps them exact, and reads eight entries
  instead of four. Hardware filtering is guaranteed only 4 bits of sub-texel precision and is
  optional for RGBA32F; it would also diverge from the CPU reference.
- **Curves sampled into a table**: 256 entries miss the 10-bit step on steep curves (2.3e-3);
  1024 pass but make the per-layer uniform block 16 KiB larger. Direct evaluation is exact and
  small (1 KiB).
- **Storing lift/gamma/gain**: no public standard and no interchange path; CDL is public and
  travels in CMX EDLs (`ASC_SOP`/`ASC_SAT`) through OTIO.
- **OpenColorIO**: a large dependency for three operations; reconsider with ADR-0006 if
  managed color (camera log, ACES) becomes a goal.
- **Grading in linear light**: LUTs and curves would not mean what their authors made them for.
- **FFmpeg's `lut3d` filter**: CPU-side, outside the compositor; its parser is internal.

## Consequences

- Grading clamps to [0, 1]: no HDR or wide-gamut grading until ADR-0006 defines those stages.
- Cost measured on Intel Iris Xe at 1080p: about +1 ms for CDL, curves and a 33³ LUT together
  (+0.8 to +1.2 ms across runs, including upload noise); curves are the costliest stage.
- The project format (ADR-0007) must persist LUT references like media: path relative to the
  project, absolute path and a fingerprint, plus the CDL and curve points.
- An imported CDL (EDL, OTIO) maps onto the model without conversion.
