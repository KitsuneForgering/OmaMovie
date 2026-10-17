## QA — image, temporal and differential oracles

Keep the CPU compositor as the mathematical reference for Vulkan on deterministic
scenes. Expand by shipped feature: transforms, crop, fit/fill/native, alpha, blend
modes, effects, color adjustments, transitions, keyed values, scaling, color spaces,
unusual and odd dimensions, then supported high-bit-depth paths. Compare selected
pixels and complete images with explicit tolerances justified by bit depth,
rounding, transfer functions, interpolation and pipeline order. Record device,
driver, format and chosen tolerance. A difference outside tolerance fails; do not
relax a threshold solely to obtain a pass. Keep an independently calculated small
scene for each mathematical rule so CPU and GPU cannot share the same wrong answer.

Use golden images only for stable reference scenes where an image catches a
regression a numeric assertion misses: transforms, blends, transitions, color,
captions and known layered compositions. Store source scene, color metadata and
approved image with a reason for updating it. Keep the set selective; review visual
changes individually, and retain mathematical/CPU comparisons alongside snapshots.

Add hand-calculable temporal cases at 24000/1001, 30000/1001 and 60000/1001 fps
(23.976, 29.97, 59.94 nominal), rational conversions, long timelines, repeated
frame ↔ time conversions, speed ramps, reverse, freeze, segmented time maps,
boundary frames and overflow. Check `frame → time → frame == frame` only where the
mapping is exactly reversible under the declared rounding rule. Test monotonicity
for forward maps, reverse ordering for reverse maps, constant media position during
freeze, and continuity across segment joins where promised. Include nested edit
sequences that slice segmented maps; do not invent unsupported nested map syntax.

Where a genuinely independent implementation exists, use differential checks:
FFmpeg/ffprobe for probing and decoded exports; OpenTimelineIO for an applicable
interchange subset; external color-science references or known reference files for
color. Compare only shared, specified semantics and record version, metadata and
tolerance. A second call to the same OmaMovie path is not an oracle. Differential
tests complement independent expected values and explicit regressions.

These checks extend the current compositor and time tests incrementally. A scene
builder or image-comparison helper is warranted only to remove repeated setup and
report precise pixel differences; it must not recreate the render graph or embed a
second compositor. Keep GPU checks on available validated hardware and software
fallback checks in CI; mark untested devices as unvalidated rather than silently
assuming agreement.

[← QA strategy](implementation-plan-qa.md)
