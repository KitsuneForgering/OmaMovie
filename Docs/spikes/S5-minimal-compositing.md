# S5 — Minimal compositing on library code (Intel Iris Xe, TigerLake)

- **Date:** 2026-10-04
- **Milestone:** M1 (`Docs/implementation-plan.md`), feeds M3/M4
- **Reproduce:** `make BUILD=release build/release/spikes/s5_minimal_compositing`, then
  `build/release/spikes/s5_minimal_compositing <video-a> <video-b> <image> software|hardware [frames]`
- **Raw output:** [2026-10-04-s5-minimal-compositing.txt](../Research/evidence/2026-10-04-s5-minimal-compositing.txt)

## Question

Can the library decoder and compositor produce the S5 scene (two videos and one image, with
transform, crop, opacity and YUV→RGB) at 1080p60 on this machine, and where does the time go?

## Method

- Inputs: the 1080p60 H.264 8-bit and HEVC Main 10 files from [S3](S3-decode-paths.md)
  (`testsrc2`, 10 s) and a 1920×1080 `testsrc2` PNG.
- Scene: A fills the 1920×1080 output; B is cropped, scaled to 45 %, rotated 12.5° and 70 %
  opaque; the image is a 35 % screen-blended overlay.
- Per frame, on one thread: `VideoDecoder::next()` for A and B, then
  `VulkanCompositor::render()` (submit to completion). 10 warm-up frames, 600 measured, two runs
  per path. Software: FFmpeg CPU decode and a plane upload per layer. Hardware: VA-API mapped
  into OmaMovie's Vulkan device (ADR-0004); the image is always a software frame.
- No presentation, no audio, no scheduler: decode and composite are sequential, so the sum is
  an upper bound for a pipelined player and a lower bound for anything that also presents.

## Results (two runs each; ms)

| Path | Stage | p50 | p95 | p99 | max | Frames over 16.67 ms |
|---|---|---|---|---|---|---|
| Software | decode + composite | 9.1–9.3 | 10.6–20.2 | 21.5–22.1 | 40–60 | 22–37 of 600 |
| VA-API → Vulkan | decode + composite | 8.2–8.3 | 9.6–10.0 | 9.9–12.3 | 21 | 3–4 of 600 |

Split for the hardware path: decode of A + B 3.1–3.2 ms p50, composite 5.5–5.6 ms p50. With
software decode, FFmpeg's frame threads decode ahead, so `next()` returns in about 0.1 ms and
the cost shows up as CPU contention: composite (which includes three uploads) is 9.3 ms p50 with
a long tail to 21–24 ms. Exploratory runs before the recorded ones gave 78–121 software frames
over budget, so the software tail varies strongly between runs.

## Findings

1. **The S5 scene runs on library code.** A PNG layer needed a fix first: the compositor
   rejected RGB sources. The decoder now rearranges packed RGB, palette and grey frames into
   planar GBR (lossless, alpha dropped for now), and the compositor reads GBR planes with a
   permutation matrix; untagged RGB is sRGB with BT.709 primaries. Tested to match the PNG
   exactly on the CPU reference and within tolerance on the GPU
   (`tests/compositor`, "draws RGB images as sRGB").
2. **With hardware decode, the median frame fits 1080p60 with margin, but the tail does not
   meet the M4 gate:** 3–4 of 600 frames exceeded 16.67 ms and p99 reached 12.3 ms in a
   sequential loop without presentation. The M4 gate (p99 end-to-end ≤ 16.67 ms) needs the
   real scheduler and presentation before any claim.
3. **Software decode at 1080p60 has a long tail** (p95 up to 20 ms) caused by decode threads
   competing with the upload and submit; the median alone would hide it.
4. **The still image is uploaded on every render.** A 1080p GBR image is about 6 MB per frame
   on the software path's critical section. Caching uploaded planes for a frame that does not
   change (same `VideoFrame` and generation) is the obvious next optimization; it is not done
   here because the M4 profile has not shown it to dominate.

## Limits

One machine, synthetic content, no thermal or frequency control, two recorded runs per path.
No presentation or Qt render thread (that is M4's measurement). Image alpha is not supported
yet; a logo with transparency renders opaque.
