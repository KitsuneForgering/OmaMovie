# Hardware strategy: synthesis

> Synthesis of [`vulkan.md`](vulkan.md), [`cuda.md`](cuda.md), [`opencl.md`](opencl.md) (OpenCL
> removed) and [`omarchy-integration.md`](omarchy-integration.md). It is a proposal for
> discussion: it becomes a rule only after it enters `CLAUDE.md` or an ADR.

Goal: **use all available hardware** (decode, encode and compute on Intel, AMD and NVIDIA)
**without taking frames out of VRAM** and without requiring users to install anything beyond
what Omarchy already installs.

---

## 1. Proposed architecture

```
                         ┌──────────────── libs/gpu ─────────────────┐
                         │  single VkInstance / VkDevice (Vulkan-Hpp) │
                         │  queue lock · VMA · imported memory        │
                         └───────┬──────────────┬──────────────┬──────┘
                                 │              │              │
        ┌────────────────────────▼──┐   ┌───────▼────────┐  ┌──▼──────────────────┐
        │ libs/media (FFmpeg 9)      │   │ libs/compositor│  │ apps/omamovie (Qt)   │
        │ hwcontext_vulkan on        │   │ render graph → │  │ QQuickGraphicsDevice │
        │ OmaMovie's device          │   │ Vulkan (+ libplacebo?)│ ::fromDeviceObjects│
        │                            │   │ ComputeBackend │  │ QSGVulkanTexture     │
        │ Decode:                    │   │  ├ Vulkan (default)│ ::fromNative       │
        │  Intel/AMD: Vulkan Video   │   │  ├ CPU (reference)                      │
        │             or VA-API→map  │   │  └ CUDA (optional, PTX)                 │
        │  NVIDIA: NVDEC→interop     │   │                                         │
        │          or Vulkan Video   │   └────────────────┘  └─────────────────────┘
        │ Encode:                    │
        │  Intel/AMD: Vulkan/VA-API  │        AVVkFrame (one timeline semaphore per image)
        │  NVIDIA: NVENC             │        = OmaMovie's Frame, without copies
        │ Intermediates: ProRes/FFv1 │
        │  in compute shaders        │
        └────────────────────────────┘
```

---

## 2. Role of each API

| API | Role | Required? |
|---|---|---|
| **Vulkan (Vulkan-Hpp)** | Single device; compositing; preview; default compute for effects; video decode/encode on AMD/Intel; compute codecs (ProRes, FFv1) | **Yes** |
| **VA-API** | Mature decode/encode on Intel/AMD; mapped to Vulkan by FFmpeg | Yes (through FFmpeg), chosen at runtime |
| **NVDEC/NVENC** | Decode/encode on NVIDIA; NVENC is much faster than Vulkan encode | Yes on NVIDIA (through FFmpeg, no toolkit) |
| **CUDA (driver API)** | NVDEC → Vulkan interop; optional kernels | Loaded at runtime, NVIDIA only |
| **CUDA (kernels)** | Effects with a measured gain on NVIDIA | Optional, optional build |
| ~~OpenCL~~ | **Removed** (decision of 2026-10-02, see `opencl.md`) | No |
| **libplacebo** | Candidate for color, tone mapping, scaling, LUTs | To be decided (ADR) |

---

## 3. Runtime selection

At startup, `libs/gpu` + `libs/media` build a **capability table** (logged in the `gpu`
category and shown on a diagnostics screen):

| Question | Source |
|---|---|
| Which codecs/profiles/bit depths does Vulkan Video decode and encode? | `vkGetPhysicalDeviceVideoCapabilitiesKHR` |
| What does VA-API offer? | `vaQueryConfigProfiles` / FFmpeg |
| Is NVDEC/NVENC present? | Presence of `libnvcuvid`/`libnvidia-encode`, FFmpeg |
| Is CUDA present? | `dlopen` of `libcuda` |
| Which GPU is primary (hybrids)? | User choice or a heuristic, without waking the dGPU needlessly |

For each stream, the decode path is chosen by an **ordered, testable policy** (e.g. Intel H.264
→ VA-API if the benchmark favors it, else Vulkan Video → software). A software fallback is
always possible, and every degradation is logged (`CLAUDE.md` §19).

---

## 4. Changes relative to the current `CLAUDE.md`

Proposals (pending the maintainer's approval):

1. ~~OpenCL as the generic backend~~ **Applied on 2026-10-02**: OpenCL removed; Vulkan Compute
   is the generic backend (`CLAUDE.md` §4 and §9.3).
2. **Explicit Vulkan-Hpp** in the stack: `vk::raii`, no exceptions (`VULKAN_HPP_NO_EXCEPTIONS` +
   `VULKAN_HPP_RAII_NO_EXCEPTIONS` with `std::expected`), include confined to
   `libs/gpu`/`libs/compositor`. Affects §4 and §22.
3. **A single Vulkan device shared with FFmpeg and Qt** as an invariant. Affects §7 and §9.4.
4. **CUDA policy**: NVDEC/NVENC through FFmpeg without the toolkit; kernels only through PTX
   (clang) + `dlopen`; optional CUDA build. Affects §4 and §9.3.
5. **Hybrid laptops**: one primary device per pipeline. Affects §7.
6. **Omarchy integration**: app_id `omamovie`, opacity rule, theme through
   `omarchy-theme-color`, an isolated platform module. Affects §11.
7. **The `vulkan-headers` package** (and `vulkan-tools` for diagnostics) in the build dependencies.

---

## 5. Validation spikes (before the definitive code)

Each spike has a measurable criterion. They replace the technical part of the "first
experiment" in `CLAUDE.md` §24.

| # | Spike | Success criterion | Hardware |
|---|---|---|---|
| S1 | `vulkaninfo` + `vainfo`: capability inventory on the Iris Xe | Documented codec × API table | Iris Xe |
| S2 | FFmpeg 9 using a `VkDevice` created by OmaMovie (Vulkan-Hpp) | H.264/HEVC/AV1 decoded into `AVVkFrame` without readback | Iris Xe |
| S3 | Vulkan Video vs. VA-API→map: frame time, CPU, power | Numbers per codec; selection policy defined | Iris Xe |
| S4 | Qt Quick on the same device showing the compositor image | 60 fps preview without a CPU copy; queue lock without deadlock | Iris Xe |
| S5 | Compositing 2 videos + 1 image (transform, crop, opacity, YUV→RGB) | Frame time measured at 1080p60 | Iris Xe |
| S6 | libplacebo on OmaMovie's images | Color conversion/scaling without copies; decision for the ADR | Iris Xe |
| S7 | NVDEC → Vulkan (interop) vs. Vulkan Video on NVIDIA | NVIDIA path chosen by measurement | **Needs an NVIDIA machine** |
| S8 | AMD (RADV) | Same measurements as S3 | **Needs an AMD machine** |

---

## 6. Main risks

| Risk | Mitigation |
|---|---|
| Queue synchronization between Qt, FFmpeg and the compositor | S4 early; a single lock mechanism; ADR-0005 |
| Driver differences in DMA-BUF/modifiers | Use FFmpeg's mapping; fallback with a logged GPU→GPU copy |
| CUDA does not accept wait-before-signal | The scheduler guarantees the order; specific tests |
| No AMD/NVIDIA hardware for testing | Plan test machines or contributors; CI only covers software paths |
| Vulkan-Hpp compile cost | Confined include + PCH; evaluate the `vulkan` module |
| libplacebo not accepting external images as expected | S6 before deciding |

---

## Suggested next steps

1. The maintainer decides the remaining points of §4 (mainly libplacebo).
2. Update `CLAUDE.md` with whatever is approved.
3. Install `vulkan-headers vulkan-tools libva-utils` and run S1.
4. Continue with S2–S4 (M1 in `Docs/implementation-plan.md`).
