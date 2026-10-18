### Video effects ahead of order (2026-10-03)

Done: Color (exposure, contrast, saturation, temperature), Crop and framing
(fit, edges, position, scale, rotation), Effects (six filter looks with previews,
soften/sharpen), transitions (ADR-0011), grading (ADR-0012, 2026-10-04: lift/gamma/gain
wheels stored as ASC CDL, monotone curves, tetrahedral 3D LUTs from `.cube` files with a fuzz
target). Not done: "Auto" color, Ken Burns (keyframes), on-viewer handles, the cut editor
(double click on a junction), scopes. Blur reworked (2026-10-04, research
`docs/Research/gpu-effects.md`): the source is linearized and box-reduced once (across, then down), the gaussian
runs on the reduced image (RGBA16F) and the composite reads it back bilinearly; maximum blur
went from 52.6 to about 3 ms at 1080p and from 214 to 5.7 ms from a 2160p source, and its strength
no longer depends on the file's resolution (evidence §5). Ken Burns through transform keyframes (M8, partial).

**Next effect selection (proposal, 2026-10-04):** [effect research](Research/post-production-effects.md)
compares the editors already researched with OmaMovie's existing tools. Pilot a
region mask with opaque cover/local blur or highlight and a focus/zoom preset
that emits editable transform keys. Compare crop/manual keys and inspect every
frame of the decoded export before claiming a sensitive area is hidden. Audit
Vintage, Sepia, Cool and Warm against saved presets; remove a redundant look
from new-project discovery only with a tested migration for old projects.
Chroma key follows PiP/alpha handling. Stabilization, video denoise and automatic
tracking remain footage/performance pilots, not committed built-in effects.

[← Implementation plan](implementation-plan.md)
