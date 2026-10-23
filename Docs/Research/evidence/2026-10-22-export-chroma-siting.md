# Export chroma siting — 2026-10-22

**Classification:** CORRECTNESS + QUALITY, with its performance cost. Found while comparing
OmaMovie's libx264 output with melt's on the cross-editor `whole` scene: identical x264
settings (the SEI strings match: medium, CRF 23, no B-frames, keyint 250), yet OmaMovie's file
was 11% smaller and its chroma further from the source.

Host: i5-1135G7 / Iris Xe, Mesa 26.2.4, iHD 26.3.5, FFmpeg n9.0.2, `intel_pstate` `powersave`,
EPP `balance_performance`, AC. Release builds of `d98d3b3` (A) and A plus this change (B).
Fixture `xed_1080p24_bt709_noise-v3.mp4` (sha256 `b555007f…`, 479 frames, tagged
`chroma_location=left`), `export_bench`; PSNR by FFmpeg against the source.

**Cause.** `nv12_bt709.comp` and `nv12_planes.comp` took each chroma sample as the mean of its
2x2 block (centre siting), while the encoder tags the stream `left`, H.264's default, so every
decoder put the colour half a pixel to the right. Input already honoured the source's siting
(`layer_params.cpp`). B samples left: a [1 2 1] / 4 filter about the even column, the edge
repeated, and the mean of the two rows; both shaders share the code, so `gpu-input` and the
readback still give byte-identical movies (checked with `cmp`).

| libx264 route, 1080p | PSNR Y | U | V | File |
|---|---|---|---|---|
| A (centre) | 44.40 | 36.61 | 34.68 | 17,290,827 B |
| B (left) | 44.40 | 37.34 | 35.79 | 16,958,534 B |
| melt 7.42 (same x264 settings) | 44.77 | 38.90 | 37.83 | 19,468,714 B |

The VA-API route with B reads Y 47.70, U 38.37, V 36.45 dB. The remaining gap to melt is
expected: melt hands the decoded YUV to x264 unchanged, while OmaMovie composites in linear RGB
(ADR-0006), so chroma is resampled up on input and down on output.

**Cost.** Six alternating rounds on `dec_h264_2160.mp4` (4K, 72 frames, VA-API): render
13.43 → 13.90 ms/frame median (+3.5%), wall 1360 [1310–1389] → 1330 [1279–1488] ms (noise),
load 1.2–2.7. `tests/compositor/test_nv12.cpp` now checks left siting against its CPU
reference (1 level; the centred shader is 52 levels off it).

**CPU reference path (same day).** Without a Vulkan device the exporter converts the CPU
compositor's RGBA with swscale, whose default destination siting is centre: a direct probe
(gray RGBA, one red column at x = 3, to NV12) put all of its Cr in chroma column 1 by default
and spread it over columns 1 and 2 with `dst_h_chr_pos = 0`. `VideoWriter` now sets
`dst_h_chr_pos = 0`, `dst_v_chr_pos = 128`. On a 640x360, 48-frame cut of the fixture
(`chroma_location=left`), libx264, `VK_ICD_FILENAMES=/nonexistent.json`:

| CPU path against | U before → after | V before → after |
|---|---|---|
| the source | 34.68 → 34.47 dB | 32.09 → 32.14 dB |
| the GPU path's export of the same clip | 38.74 → 42.44 dB | 36.55 → 40.91 dB |

So siting was not why the CPU path's chroma trails the GPU path's against the source
(36.66 / 34.60 dB there); the change makes the CPU continuation of a device lost mid-export
match the GPU frames it follows. The cause of the remaining 2 dB is not identified.
The exporter also stopped calling this path's input "NV12 readback".

**Open: sources without a chroma location (2026-10-22).** FFmpeg reports `left` for H.264
and HEVC that do not signal one (both codecs' default), so input and output agree for them.
AV1 and VP9 report `unspecified` (`av1_opus.mkv`, `dec_av1_2160.mp4`, `dec_vp9_2160.webm`),
which `layer_params.cpp` reads as centre while the export now writes left, as its stream
says. That is right when such a source really is centre-sited (FFmpeg's own `yuv420p`
conversion, which made these fixtures, is centre by default) and half a pixel off when it is
left-sited, where the former centre output had cancelled the input's error. Which convention
real AV1/VP9 sources (screen recorders, cameras, browsers) follow has not been researched or
measured; settle it from encoder sources and a few real files before changing the input rule.
