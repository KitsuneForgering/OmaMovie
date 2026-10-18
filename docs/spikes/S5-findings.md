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

[Return to S5-minimal-compositing.md](S5-minimal-compositing.md)
