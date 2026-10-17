## Quantitative checks

Locally executed **arithmetic**, not a hardware benchmark. Assumptions: tightly packed even
4:2:0 frames, no padding/compression, decimal bandwidth, binary memory units. For `W × H`
pixels: NV12 = `1.5WH` bytes, P010 = `3WH`, RGBA16F = `8WH`. One full-frame transfer at rate
`f` costs `Bf` bytes/s; readback plus re-upload doubles that payload, not necessarily measured
time. Shared system memory on an iGPU still has traffic/synchronization costs.

[Calculation output](evidence/2026-10-03-calculations.txt): 4K RGBA16F = 63.28125 MiB;
one transfer at 60 fps = 3.981312 GB/s; 3 streams × 8 frames = 1518.75 MiB excluding other
allocations. 1080p60's presentation interval is 16.6667 ms. A 3 ms isolated compositor sample
does not establish decode/audio/UI/presentation deadlines or tail latency.

Reproduce the capacity arithmetic:

```python
for w, h in [(1920, 1080), (3840, 2160)]:
    for name, bpp in [('NV12', 1.5), ('P010', 3), ('RGBA16F', 8)]:
        b = int(w * h * bpp)
        print(w, h, name, b, b / 2**20, b * 60 / 1e9)
print(1000 / 60, 3840 * 2160 * 8 * 3 * 8 / 2**20)
```

A constant 2× speed over 10 s consumes 20 s of source; a linear speed ramp 1×→2× over
10 s consumes 15 s (`integral s(t) dt`). A frozen 10 s domain consumes zero source duration.
These dimensional/boundary cases refute a duration rule based only on source displacement.
The arithmetic is locally checked; the TimeMap implementation remains proposed.

[Return to skeptical-review.md](skeptical-review.md)
