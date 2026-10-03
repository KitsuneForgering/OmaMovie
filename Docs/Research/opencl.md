# OpenCL in OmaMovie: evaluation

> **Decision (2026-10-02): OpenCL removed from the project.** Vulkan Compute is the generic
> backend; CUDA remains an NVIDIA specialization. This document stays as the record of the
> analysis behind the decision (`CLAUDE.md` §9.3).

> Research done on 2026-10-02. Sourced facts are linked in the [Sources](#sources) section.
> Items marked **(verify)** need confirmation.
>
> Verified locally: `ocl-icd` (the loader) is installed, but **no ICD is registered**
> (`/etc/OpenCL/vendors` does not exist). The Omarchy 4.0.4 installer **does not install an
> OpenCL runtime** for any vendor. Packages available on Arch: `opencl-mesa` (rusticl),
> `intel-compute-runtime`, `rocm-opencl-runtime`, `opencl-nvidia`.

---

## 1. Summary

`CLAUDE.md` described OpenCL as the "generic compute backend". The research indicates that,
**on Omarchy and for a GPU-first pipeline, Vulkan Compute fills that role better**, and OpenCL
should be an **optional, later** backend, subject to two factors:

1. **Zero-copy interop with Vulkan.** The decisive point. In rusticl (Mesa)
   `cl_khr_external_memory` was still in development at the end of 2025. Without it, an OpenCL
   effect forces a frame copy, which violates the core principle of `CLAUDE.md` §7.
2. **Availability.** The OpenCL runtime is not installed on Omarchy. Vulkan always is.

This **diverged from `CLAUDE.md` at the time** and was decided by the maintainer (see §6).

---

## 2. State of OpenCL on Linux (2026)

| Runtime | Hardware | Status |
|---|---|---|
| **rusticl** (Mesa, `opencl-mesa`) | Intel (iris), AMD (radeonsi), Zink (any Vulkan), Asahi, llvmpipe | **OpenCL 3.1** in Mesa 26.2. Enabled by default on some drivers (radeonsi, asahi, freedreno, zink); others need `RUSTICL_ENABLE` |
| **intel-compute-runtime** (NEO) | Intel | Intel's official runtime (OpenCL + Level Zero) |
| **ROCm** (`rocm-opencl-runtime`) | AMD | AMD's official runtime |
| **NVIDIA** (`opencl-nvidia`, ~103 MiB) | NVIDIA | Supports the Vulkan interop extensions |

### Interop with Vulkan
- Khronos extensions: `cl_khr_external_memory` (+ `_opaque_fd`, `_dma_buf`), `cl_khr_semaphore`,
  `cl_khr_external_semaphore` (+ `_opaque_fd`). They allow sharing memory and synchronizing with Vulkan.
- **NVIDIA** documents using these extensions with its OpenCL.
- **rusticl**: `cl_khr_external_memory` listed as work in progress at XDC 2025. **(verify the state in Mesa 26.2)**.
- **intel-compute-runtime**: support for the external extensions not confirmed in this research **(verify)**.
- There is an official Vulkan sample, "Cross vendor OpenCL and Vulkan interoperability".

---

## 3. The Blender lesson

Blender **removed OpenCL from Cycles in version 3.0** (the Cycles-X rewrite). Reasons given:
- a limited kernel implementation in OpenCL;
- **driver bugs** from certain vendors;
- a **stalled standard**;
- hard maintenance of code separate from the C++/CUDA path.

Afterwards AMD, Apple and Intel contributed **HIP, Metal and oneAPI** backends.

**Lesson for OmaMovie:** maintaining an extra compute backend is expensive and depends on driver
quality. Blender preferred native per-vendor backends over a "generic" backend that worked
poorly everywhere.

Counterpoint: OpenCL has evolved since 2021 (OpenCL 3.0/3.1, a mature rusticl), and Darktable
still uses OpenCL successfully **(general knowledge)**.

---

## 4. Comparison: Vulkan Compute vs. OpenCL for OmaMovie

| Criterion | Vulkan Compute | OpenCL |
|---|---|---|
| Available on Omarchy by default | **Yes** (Vulkan drivers always installed) | No |
| Direct access to compositor images | **Yes, same device and memory** | Only with interop extensions |
| Zero-copy interop on Mesa | Not applicable (native) | In development (rusticl) |
| Kernel language | GLSL/HLSL/Slang → SPIR-V | OpenCL C / C++ for OpenCL → SPIR-V |
| Convenience for general compute | Lower (more verbose, descriptors) | **Higher** (simpler buffer and kernel model) |
| Precision and numeric features | Depends on extensions | Good precision support and math builtins |
| Ecosystem of ready kernels | Video shaders (libplacebo, FFmpeg) | Scientific libraries, Darktable, FFmpeg OpenCL filters |
| Use in FFmpeg | Vulkan filters + decode/encode | OpenCL filters (`hwcontext_opencl`, with VA-API interop on Intel) |

---

## 5. Where OpenCL could still make sense

- **Reusing existing kernels or libraries** written in OpenCL, when rewriting them in Vulkan is not worth it.
- **Non-visual computation** (audio analysis, scene detection, statistics) where OpenCL's
  simpler model helps and the data does not need to live in a Vulkan image.
- **NVIDIA**, if an effect is simpler in OpenCL than in CUDA and interop is available.

None of these cases exists in OmaMovie today.

---

## 6. Recommendation and decision

Recommendation presented to the maintainer:

1. **Vulkan Compute is the default generic backend** of `ComputeBackend`. Every effect has a Vulkan implementation.
2. `CpuBackend` stays as the correctness reference and for tests.
3. **OpenCL becomes an optional backend**, implemented only when:
   - there is a concrete case (one of the items in §5); and
   - zero-copy interop with Vulkan has been verified on the target runtime (test
     `cl_khr_external_memory_dma_buf`/`_opaque_fd` on rusticl and NEO).
4. The `ComputeBackend` interface keeps allowing OpenCL (the door stays open), and the OpenCL
   runtime is **detected at runtime** (`dlopen` of the ICD loader), never required.

**Decision (2026-10-02):** the maintainer chose to **remove OpenCL entirely**. `CLAUDE.md` §4
and §9.3 were updated: Vulkan Compute is the generic backend, and adding OpenCL back requires an
explicit decision and an ADR.

---

## Sources

- [Phoronix: Rusticl ready with OpenCL 3.1 on Radeon, Intel Iris & Zink](https://phoronix.com/news/OpenCL-3.1-Same-Day-Rusticl)
- [Comss: Mesa 26.2.0 with OpenCL 3.1](https://www.comss.ru/page.php?id=21518)
- [Phoronix: Rusticl has turned out remarkably well (XDC 2025)](https://phoronix.com/news/Rusticl-XDC2025)
- [Phoronix: Mesa 24.3 build option to enable Rusticl by default](https://phoronix.com/news/Rusticl-Default-Mesa-24.3)
- [Phoronix: Mesa Git makes it easier activating Rusticl (RUSTICL_ENABLE)](https://www.phoronix.com/news/Mesa-RUSTICL_ENABLE)
- [Phoronix: Rusticl adds cl_khr_gl_sharing](https://www.phoronix.com/news/Rusticl-cl_khr_gl_sharing)
- [Khronos: OpenCL 3.0 extensions for NN inferencing and OpenCL/Vulkan interop](https://www.khronos.org/blog/khronos-releases-opencl-3.0-extensions-for-neural-network-inferencing-and-opencl-vulkan-interop)
- [Khronos Registry: cl_khr_external_memory](https://registry.khronos.org/OpenCL/sdk/3.0/docs/man/html/cl_khr_external_memory.html)
- [NVIDIA: Using semaphore and memory sharing extensions for Vulkan interop with OpenCL](https://developer.nvidia.com/blog/using-semaphore-and-memory-sharing-extensions-for-vulkan-interop-with-opencl)
- [Vulkan Samples: Cross vendor OpenCL and Vulkan interoperability](https://docs.vulkan.org/samples/latest/samples/extensions/open_cl_interop/README.html)
- [Phoronix: OpenCL 3.0.9 extensions for Vulkan interop](https://www.phoronix.com/news/OpenCL-3.0.9-Extensions)
- [Blender 3.0 release notes: Cycles](https://wiki.blender.org/release_notes/3.0/cycles/)
- [Blender: Next level support for AMD GPUs](https://code.blender.org/2021/11/next-level-support-for-amd-gpus/)
- [GPUOpen: Blender Cycles AMD GPU](https://gpuopen.com/blender-cycles-amd-gpu/)
- Local Omarchy 4.0.4 files: `/usr/share/omarchy/install/hardware/`
