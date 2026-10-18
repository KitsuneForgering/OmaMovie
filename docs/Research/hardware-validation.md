## 5. Experiments and gates

| Spike | Review status | Required additional evidence |
|---|---|---|
| S1 | Prior Intel report, not rerun | Retain capability vs executed-codec distinction; raw output/provenance |
| S2 | Prior single-submitter report, not rerun | Validation layers, chroma/multiple frames, lifetime/cancellation |
| S3 | Pending | Repeated equivalent workloads; decode + import + consumer, encode quality |
| S4 | Pending; Q1 identified | Queue retrieval, submission/lifetime protocol, display conversion |
| S5 | Library tests contain timings | End-to-end playback, percentiles/drops, independent color oracle |
| S6 | Pending | Installed libplacebo contracts, image-quality and copy comparisons |
| S7 | Pending; NVIDIA unavailable | Decode+interop+composition and export on actual NVIDIA |
| S8 | Pending; AMD unavailable | Same criteria on actual AMD |

Protocols and stop criteria live in [the audit](skeptical-review.md#validation-protocols).
No pending experiment is relabeled as completed by this documentary review.

## 6. Capacity model

Use measured resource budgets, not only throughput. Tightly packed 4K RGBA16F is 63.28 MiB
per frame; three streams with eight such cached frames consume about 1.48 GiB before decoder
pools, intermediate targets and padding. This is a calculated lower-bound illustration,
not measured VRAM usage. [Inputs and calculations](skeptical-review.md#quantitative-checks).
On integrated GPUs, GPU-local allocation does not imply physically separate VRAM.

## 7. Application decision

Continue bounded Intel correctness work. Resolve Q1 and SDR/display handling before wiring
the final UI. Support claims must name tested hardware/drivers/media; untested AMD/NVIDIA
remain planned. Choosing a proxy codec also needs storage, seek, quality and compatibility
measurements; GPU compute decode alone does not justify FFv1/ProRes as the default.

[Return to hardware-strategy.md](hardware-strategy.md)
