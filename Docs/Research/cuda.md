# CUDA and the NVIDIA path in OmaMovie

> Research done on 2026-10-02. Sourced facts are linked in the [Sources](#sources) section.
> Items marked **(verify)** need confirmation.
>
> Verified locally: `nvidia-utils` 610.57 provides `libcuda.so`, `libnvcuvid.so` (NVDEC) and
> `libnvidia-encode.so` (NVENC). The system FFmpeg was built with
> `--enable-nvdec --enable-nvenc --enable-cuda-llvm`. Arch's `cuda` package (toolkit) is
> **4.71 GiB** and is not installed.

---

## 1. Summary

"Using CUDA" in OmaMovie means **three different things**, with very different costs:

| What | Needs the CUDA toolkit? | Gain | Recommendation |
|---|---|---|---|
| **NVDEC/NVENC** (hardware decode/encode) | **No**. FFmpeg loads the driver libraries at runtime | High: NVENC is about 5x faster than Vulkan Video encode on NVIDIA | **Yes, early** |
| **CUDA ↔ Vulkan interop** (bringing NVDEC frames to the Vulkan compositor) | No, it uses the driver API (`libcuda`) | Needed to keep frames in VRAM | **Yes**, together with NVDEC |
| **Our own CUDA kernels** (effects) | Only at build time (headers/libdevice); at runtime the driver is enough | Depends on the effect; Vulkan Compute covers most | **Only with a measured gain** |

Conclusion: on NVIDIA the biggest gain comes from NVDEC/NVENC, which **requires neither
distributing nor building with the toolkit**. CUDA kernels are a later specialization.

---

## 2. NVDEC and NVENC

- Part of the NVIDIA Video Codec SDK. FFmpeg accesses them through `ffnvcodec-headers`, which
  **load `libcuda`/`libnvcuvid`/`libnvidia-encode` dynamically**. There is no build-time link
  against NVIDIA libraries **(verify: known ffnvcodec behavior)**.
- A frame decoded by NVDEC lives in **CUDA memory** (FFmpeg's `cuda` hwaccel).
- NVENC accepts CUDA frames directly.
- Comparison reported on the NVIDIA forum: Vulkan Video encode ~194 fps vs. NVENC ~910 fps.

### Omarchy and NVIDIA
Omarchy installs `nvidia-open-dkms`, `nvidia-utils` and `libva-nvidia-driver` (GPUs with GSP)
and sets `LIBVA_DRIVER_NAME=nvidia` and `NVD_BACKEND=direct`. So VA-API exists on NVIDIA, but
through a translation layer on top of NVDEC.

**For OmaMovie:** on NVIDIA, use NVDEC/NVENC directly (FFmpeg's `cuda` hwaccel), not the VA-API
layer, which adds a translation step and has known DMA-BUF/NV12 limitations when imported by
Vulkan (see `vulkan.md` §4).

---

## 3. CUDA ↔ Vulkan interop

The compositor is Vulkan, but the NVDEC frame is in CUDA memory. To avoid going through RAM:

1. OmaMovie creates an **exportable** Vulkan image/buffer (`VK_KHR_external_memory_fd`, opaque fd handle).
2. CUDA imports that memory: `cuImportExternalMemory` (driver API) or `cudaImportExternalMemory` (runtime API).
3. The NVDEC surface is copied into it with a **device-to-device copy** (it stays in VRAM; not
   zero-copy, but no download).
4. Synchronization through a **timeline semaphore** exported from Vulkan and imported into CUDA
   (`cudaImportExternalSemaphore`, `cudaSignalExternalSemaphoresAsync`, `cudaWaitExternalSemaphoresAsync`).

**Important restriction:** in CUDA it is **illegal to wait before the corresponding signal has
been issued**. Vulkan timeline semaphores' wait-before-signal does not hold on the CUDA side.
OmaMovie's scheduler must guarantee the order (signal submitted before the wait is submitted in CUDA).

Alternatives to evaluate:
- **FFmpeg frame mapping** between the `cuda` and `vulkan` contexts **(verify whether FFmpeg 9
  supports `av_hwframe_map`/transfer CUDA→Vulkan without the CPU)**. If it does, it avoids
  writing the interop by hand.
- **Vulkan Video decode directly on NVIDIA**: removes the interop. Decode performance (not
  encode) must be measured before ruling it out.

---

## 4. Our own CUDA kernels

### When it would be worth it
- Heavy effects where CUDA has a **measured** advantage over Vulkan Compute on the same GPU
  (e.g. mature libraries such as NPP, or kernels relying on specific features).
- `CLAUDE.md` already says: CUDA only where there is a real gain, and effects do not depend
  directly on CUDA.

### Building without `nvcc` and without requiring the toolkit at runtime
The system FFmpeg uses `--enable-cuda-llvm`: its CUDA kernels are compiled by **clang to PTX**.
The same pattern works for OmaMovie:

1. `.cu` kernels compiled with **clang** to PTX (LLVM has supported CUDA since 3.9). The build
   needs the CUDA headers/libdevice (`--cuda-path`).
2. The PTX is **embedded in the binary**.
3. At runtime, `libcuda.so` is loaded with `dlopen` (it ships with `nvidia-utils`) and the PTX
   through the driver API (`cuModuleLoadData` / `cuLinkAddData`). The driver JITs it for the GPU present.

Consequences:
- End users **do not** need to install the toolkit (4.7 GiB).
- Machines without NVIDIA load nothing; the CUDA backend simply does not appear.
- The CUDA build is **optional** (a Make option), so not every developer needs the toolkit.

---

## 5. Hybrid laptops (Intel/AMD + NVIDIA)

Omarchy detects hybrid GPUs and avoids waking the dGPU needlessly (its detectors read sysfs
instead of `lspci`, because waking the GPU exceeds Hyprland's reload time budget).

**Implications for OmaMovie:**
- Device choice is a first-class decision: decoding on the dGPU and compositing/presenting on
  the iGPU implies **a copy between GPUs** over the bus.
- Proposed initial rule: **one primary device for the whole pipeline** (decode, compositing,
  preview), chosen by the user or by a heuristic (dGPU when plugged in?), and logged.
- Do not wake the dGPU just to enumerate capabilities; use sysfs/Vulkan information without
  creating a device when possible **(verify)**.

---

## 6. Licensing

- Loading `libcuda`/`libnvcuvid`/`libnvidia-encode` dynamically from the installed driver is the
  model used by FFmpeg and other free projects.
- The CUDA toolkit has its own EULA. Do not redistribute parts of the toolkit; the PTX OmaMovie
  generates is project code **(verify the EULA regarding libdevice embedded in the PTX)**.

---

## 7. Recommendations

1. **Decode/encode phase (M1–M7)**: NVDEC/NVENC through FFmpeg on NVIDIA, no toolkit.
2. **Interop**: try FFmpeg's frame mapping first; if there is no CPU-free path, implement
   external memory + timeline semaphore interop in `libs/gpu`.
3. **Benchmark**: NVDEC + interop vs. direct Vulkan Video decode on NVIDIA. Choose by measurement.
4. **Compute `CudaBackend`**: only after the Vulkan `ComputeBackend` exists and an effect has a
   measured gain. PTX through clang, `dlopen` at runtime, optional build.
5. **Test hardware**: the current machine has no NVIDIA GPU (`nvidia-utils` is installed, but
   `lspci` only shows the Iris Xe). Validating the NVIDIA path needs another machine.

---

## Sources

- [NVIDIA: CUDA Runtime API — External Resource Interoperability](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__EXTRES__INTEROP.html)
- [NVIDIA: CUDA Driver API — Module Management](https://docs.nvidia.com/cuda/archive/13.2.2/cuda-driver-api/group__CUDA__MODULE.html)
- [NVIDIA Forums: cudaWaitExternalSemaphoresAsync blocks CPU kernel launch](https://forums.developer.nvidia.com/t/cudawaitexternalsemaphoresasync-blocks-cpu-kernel-launch/329028)
- [IWOCL 2025: SYCL interoperability (external memory/semaphores)](https://www.iwocl.org/wp-content/uploads/iwocl-2025-duncan-brawley-sycl-interoperability.pdf)
- [Intel: DPC++ Compatibility Tool — migration examples (Vulkan interop)](https://www.intel.com/content/www/us/en/docs/dpcpp-compatibility-tool/developer-guide-reference/2025-2/migration-examples.html)
- [vulkane: external memory export example](https://docs.rs/crate/vulkane/0.10.1/source/examples/external_memory_export.rs)
- [LLVM: Compiling CUDA with clang](https://llvm.org/docs/_sources/CompileCudaWithLLVM.md.txt)
- [NVIDIA Forums: Building CUDA code with clang](https://forums.developer.nvidia.com/t/building-cuda-code-with-clang/28781)
- [NVIDIA: Video Codec SDK](https://developer.nvidia.com/video-codec-sdk/download)
- [Phoronix: Using NVIDIA's NVENC on Linux with FFmpeg](https://www.phoronix.com/news/MTg0NTY)
- [Phoronix: FFmpeg NVDEC-accelerated H.264 decoding](https://www.phoronix.com/news/FFmpeg-NVDEC-H264-Acceleration)
- [Phoronix: NVIDIA VA-API driver 0.0.18](https://phoronix.com/news/NVIDIA-VA-API-Driver-0.0.18)
- [NVIDIA Forums: Vulkan Video is 5x slower than NVENC](https://forums.developer.nvidia.com/t/vulkan-video-is-5x-slower-than-nvenc/373916)
- Local Omarchy 4.0.4 files: `/usr/share/omarchy/install/hardware/nvidia.sh`, `/usr/share/omarchy/default/hypr/nvidia.lua`
