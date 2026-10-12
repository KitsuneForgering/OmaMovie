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

[Return to the decision](0012-color-grading.md)
