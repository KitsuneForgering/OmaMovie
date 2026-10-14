# ADR-0006 — Working color space, SDR stages and libplacebo

**Status: Accepted (2026-10-04).** Evidence: [S5](../spikes/S5-minimal-compositing.md),
[S6](../spikes/S6-libplacebo.md), `tests/compositor`.

## Context

The compositor (M3) already works in linear light with BT.709 primaries, premultiplied alpha
and RGBA16F images, as this ADR's proposal said, and encodes an sRGB RGBA8 image for the viewer
(M4). The skeptical review asked this ADR to define the SDR input, working and display stages
and how they are validated independently of the compositor's own CPU reference, and to settle
whether libplacebo replaces our shaders. Phone recordings (a core audience) are often HLG; no
part of the pipeline tone maps.

## Decision

**Stages (SDR):**

| Stage | Rule |
|---|---|
| Input, YUV | Matrix, range, primaries and transfer from the stream. Untagged: BT.709 above 576 lines, BT.601 below; limited range. Transfers: BT.1886 (gamma 2.4) for video, sRGB and linear when tagged. |
| Input, RGB | Planar GBR from the decoder (packed RGB, palette and grey are rearranged). Untagged RGB is sRGB with BT.709 primaries at any size. Alpha is dropped until layers need it. |
| Working | Linear light, BT.709 primaries, premultiplied alpha, RGBA16F. Gamut conversion to BT.709 happens per layer before blending; effects and grading (ADR-0012) run in this space unless their own ADR says otherwise. |
| Display (viewer) | sRGB transfer (IEC 61966-2-1) to RGBA8, no dithering. |
| Export | H.264/HEVC SDR tagged BT.709 (primaries, matrix, transfer), limited range. Encoded with the inverse of the BT.1886 EOTF that decoding applies, so an ungraded clip exports with its own levels (amended 2026-10-13, below). |

**HDR (PQ/HLG):** not supported. Such sources go through the SDR path without tone mapping
(HLG looks roughly right on SDR, PQ does not) and the decoder logs one warning per file. No
HDR claim is made from RGBA16F storage. Tone mapping is a future decision with independent
vectors and a display/export contract (M9).

**libplacebo:** not adopted now. S6 showed it can run on our device and read our images
without copies, but the stages OmaMovie has are as accurate or more accurate in our shaders at
equal or lower cost, and it would add a dependency to the shipped binary. It is the first
candidate when we need HDR tone/gamut mapping, high-quality scaling for export, or dithering;
that integration must use zero-flag queues with the device's queue lock (ADR-0005) and
libplacebo's release/hold calls around FFmpeg's semaphores, as the spike does.

**Validation.** GPU output is compared with `CpuCompositor`, and independently of it:
the display encode against the sRGB formula (within 1 level), and an RGB image against the
pixels FFmpeg's own converter produces (exact). An independent YUV oracle (known vectors per
matrix/range/transfer) remains open; until it exists, claims stop at "GPU matches CPU
reference" for YUV inputs.

## Alternatives considered

[Review the alternatives](0006-alternatives.md)

## Consequences

- HDR footage looks wrong (PQ) or flatter (HLG) in the viewer and in any export until tone
  mapping exists; users only see the log so far, the UI must surface it (M6).
- RGB images are uploaded per render; S5 measured the cost and leaves caching for when a
  profile shows it matters.
- Any libplacebo integration must keep `VK_KHR_internally_synchronized_queues` disabled on
  zero-flag devices (now the `libs/gpu` behavior), because libplacebo and FFmpeg infer the
  queue flag from the extension list.

## Amendment 2026-10-13: export transfer

M7 encoded exports with the BT.709 camera OETF while decoding read BT.709 video through the
BT.1886 EOTF. The pair is not an inverse: every export darkened mid-tones (limited-range level 125
came back as 108, level 43 as 23; worst 27 levels). Colourful test patterns hid it, because
saturated primaries sit on the curve's fixed points; a gray ramp and a real Omarchy recording
showed it (M10 workflow tests). Exports now use `L^(1/2.4)` on the GPU and in the CPU table, and
keep the BT.709 tags, which players display with BT.1886. Checks: `test_export_transfer` (all 256
levels exact), `test_export_levels` (end to end within ±2 levels), `test_compositors` (GPU within
one level). Scene-referred camera footage is therefore treated as display-referred, like the
viewer already did; a scene-referred workflow would be a separate decision.
