# ADR-0022 — Hardware decode for export where validated

**Status:** Proposed 2026-10-19 (implemented on `feature/first-release`; awaits maintainer
acceptance). Moves M12 step 5 ahead of GPU-native encoder input on measured evidence.

## Context

Export constructed `FrameSource(nullptr)` and so always decoded in software, on the stated
grounds that export should not compete with the viewer's hardware decoder and that a
software encoder needs a CPU readback anyway. Since M7 the Intel export uses VA-API
encoding, and the [2026-10-19 repeats](../Research/evidence/2026-10-19-export-transfer-repeats.md)
found the staged 4K export mostly CPU-bound with the GPU about half idle. The M12 plan
deferred hardware decode until after GPU-native encoder input.

## Decision

- `ExportRequest::Decode` (`Auto`, `Software`, `Hardware`) mirrors `Encoder`. `Auto` decodes
  in hardware only where OmaMovie validated it, Intel, as the viewer and the encoder do; a
  file whose hardware decoder cannot open falls back to software in `VideoDecoder`.
- The Settings decode choice, fixed for the run, applies to export as well as the viewer.
- The export keeps its own `FrameSource` and device: it does not share the viewer's
  decoders or surfaces.
- On device loss the export replaces its frame source with a software one and decodes the
  current frame again before the CPU reference renders it.
- `ExportStats::decode_path` and `compositor_path` report the route and any change.

## Evidence and consequences

The [A/B](../Research/evidence/2026-10-19-export-hardware-decode.md) on the 4K23.976 fixture:
process CPU per frame 272 → 43 ms (median), export 9.5 → 7.1 s, output pixel-identical; two
concurrent exports both correct. GPU-resident memory rises by 150–250 MB and the video
engine carries decode and encode. Local app tests now exercise the hardware path by
default on Intel; CI without VA-API keeps the software path. The GUI smoke now plays the
viewer while it exports (Intel: both decode through VA-API, including a 10-bit HEVC source)
and requires playback to advance and the export to finish. `test_export_decode_paths.cpp`
requires 10-bit HEVC and AV1 exports decoded in hardware to hash frame for frame (ffmpeg
`framemd5`) like the same exports decoded in software; on Intel both used VA-API, and an
encoder change on one side makes it fail. A 25-minute
long export passed with the same output size and checks as software decode, at half the
CPU per frame. Not validated: other vendors, multi-stream scenes and two-hour exports.
Revert by choosing software decode in Settings or reverting this ADR's change.
