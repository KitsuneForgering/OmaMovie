## 6. Hardware matrix

| Hardware | Available | Current claim |
|---|---|---|
| Intel Iris Xe (Tiger Lake) | Yes | Validate preview and export against recorded fixtures |
| Software/CI | Yes | Validate fallback and deterministic behavior |
| AMD (RADV) | No | Unvalidated; no AMD support claim |
| NVIDIA (nvidia-open) | No | Unvalidated; no NVDEC/NVENC support claim |
| Hybrid laptop | No | Unvalidated; no power or device-selection claim |

The v0.1 decision may cover the validated Intel/software scope. AMD, NVIDIA and hybrid
support become separate gates when hardware or reliable contributor evidence is
available. Do not infer vendor support from successful compilation or another GPU.
On validated Intel preview decode, VA-API surfaces are imported through DRM PRIME/
DMA-BUF into Vulkan without an OmaMovie CPU readback; driver-internal copies were
not measured. Software fallback uploads pixels; export reads rendered frames back
before FFmpeg/VA-API encoding. Record this decode → memory → compositor → display/
encoder path and copies with each hardware benchmark; do not call all paths zero-copy.

[← Implementation plan](implementation-plan.md)
