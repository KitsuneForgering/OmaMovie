# GPU post-production effects

> Researched on 2026-10-04 with skeptical-research, algorithmic-project-review, ponytail and
> anti-ai-mediocrity. Non-normative: the decisions live in [ADR-0012](../adr/0012-color-grading.md). Local evidence:
> [evidence/2026-10-04-gpu-effects.txt](evidence/2026-10-04-gpu-effects.txt) with the
> [benchmark](evidence/2026-10-04-compositor-bench.cpp) and the
> [numerics](evidence/2026-10-04-grading-numerics.py).

Question: how to add color grading (3D LUTs, color wheels, curves), multi-pass effects and
keyframes to the Vulkan compositor without breaking its invariants (CPU reference, render
graph as plain data, no readback, timeline without GPU types).

## Conclusions

| Topic | Recommendation | Basis |
|---|---|---|
| Order | Grading first, then the multi-pass rework of blur, then keyframes | Grading fits the existing per-pixel pass; blur is the measured bottleneck |
| Wheels | Store ASC CDL (slope, offset, power per channel + saturation); the UI shows lift/gamma/gain | The mapping is exact (evidence 2a); CDL is public and travels in CMX EDLs through OTIO |
| Curves | Monotone cubic (Fritsch–Carlson) evaluated in the shader from at most 16 points | No overshoot; a 256-entry table misses the 10-bit step on steep curves (2b) and 1024 entries do not fit a per-layer UBO comfortably |
| 3D LUT | Tetrahedral interpolation with `texelFetch`, the same code path as the CPU reference | Greys stay grey (2c); four reads instead of eight; FFmpeg's default |
| Grading space | BT.709 primaries, re-encoded with the inverse of the layer's own transfer function, clamped to [0, 1] | The LUT and curves see the file's code values, as in a Rec.709 timeline; CDL's ASC style clamps too |
| `.cube` parser | 3D only, red fastest, `DOMAIN_MIN/MAX` and Resolve's `LUT_3D_INPUT_RANGE`, size 2–65, fuzzed | Both keyword families exist in the wild; size 256 would be 268 MB of floats |
| ComputeBackend | Not now | No effect needs a second backend; CLAUDE.md §9.3 asks for CUDA only on a measured gain |
| Blur | Linearize the source once per layer, then blur a downsampled pyramid | Measured 52.6 ms (1080p) and 214 ms (4K source) at maximum; cost is source pixels × taps. **Done** as a separable box reduction (evidence §5): about 3 ms and 5.7 ms |
| Keyframes | Hold/linear/ease, clip-relative `RationalTime`, evaluated in `timeline::evaluate` | Weak external evidence; the render graph already receives evaluated parameters (§9.1). **Implemented for the transform** (2026-10-04) with one deviation: keys sit in the clip's source time, not clip-relative time, so split/trim/slip/speed keep the motion on the picture without changing any edit command; lookup is a binary search, O(log k) |

## Evidence

**Documented.** OCIO's Resolve cube reader reads red-fastest data, optional
`LUT_3D_INPUT_RANGE`, 1D shaper plus 3D, and rejects wrong row counts
([source](https://raw.githubusercontent.com/AcademySoftwareFoundation/OpenColorIO/main/src/OpenColorIO/fileformats/FileFormatResolveCube.cpp)).
FFmpeg's `lut3d` reads `DOMAIN_MIN/MAX` and defaults to tetrahedral interpolation
([source](https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavfilter/vf_lut3d.c));
its ticket #10431 records that the Adobe 1.0 specification names `DOMAIN_MIN/MAX`, not the
`*_INPUT_RANGE` keywords ([ticket](https://ffmpeg.org/pipermail/ffmpeg-trac/2023-June/066367.html)).
OCIO's CDL "ASC" style clamps to [0, 1] and uses Rec.709 luma for saturation
([docs](https://opencolorio.readthedocs.io/en/latest/api/transforms.html)). OTIO's CMX 3600
adapter carries `ASC_SOP`/`ASC_SAT` ([adapters](https://opentimelineio.readthedocs.io/en/v0.15/tutorials/adapters.html)).
A SMPTE study reports tetrahedral matching trilinear quality with LUTs 20–25% smaller
([abstract](https://journal.smpte.org/periodicals/SMPTE%20Motion%20Imaging%20Journal/129/2/17/)).

**Not read directly.** The Adobe Cube LUT Specification 1.0 PDF (the archived copy was not
reachable); its content above comes through the FFmpeg ticket and the OCIO reader.

**Observed locally.** Render times and numerics in the evidence file. The benchmark is
synchronous and includes the software-frame upload; one GPU only.

**Algorithmic analysis.** Blur cost is `source_px × (2r + 1) × 2` taps plus a composite reading
the blurred image. Pass 1 decodes YUV and runs three `pow` per tap (`texel()` in
`shaders/composite.comp`): 387 `pow` per pixel at r = 64. `make_detail` clamps sigma to 21.3 px
(`src/look.cpp`), so at maximum a 2160p source gets half the relative blur of a 1080p one: the
strength depends on the file's resolution. Both are fixed by a linear-RGB intermediate and a
downsampled pyramid (cost independent of the radius), at 16 MB (1080p) or 66 MB (2160p) of
RGBA16F per layer that needs it.

## Open checks

- End-to-end grading cost in the viewer. The isolated 1080p benchmark measured
  +0.83 ms and +1.24 ms in two runs for all three stages, around a +1 ms target;
  it includes a synchronous software-frame upload and does not measure presentation.
- AMD/NVIDIA behavior; only Intel was measured.
- Keyframe interpolation against FCPXML/Resolve exports once importers exist.
