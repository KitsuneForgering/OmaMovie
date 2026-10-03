# Vulkan and Vulkan-Hpp in OmaMovie

> Research done on 2026-10-02. Sourced facts are linked in the [Sources](#sources) section.
> Items marked **(verify)** are inferences or general knowledge that must be confirmed in code
> or documentation before becoming a decision.
>
> Environment verified locally: Mesa 26.2.2 (`vulkan-intel`), Vulkan loader 1.4.357, FFmpeg
> 9.0.1 (with `--enable-vulkan --enable-libplacebo`), Qt 6.11.2, Intel Iris Xe GPU (TigerLake).
> Not installed: `vulkan-headers` (which contains `vulkan.hpp`), `vulkan-tools`.

---

## 1. Summary

Vulkan is OmaMovie's **backbone**. It serves four different roles, and for two of them the
ecosystem matured a lot in 2025–2026:

| Role | Status |
|---|---|
| **Compositing and preview** | Mature. Qt Quick runs on Vulkan and accepts an external `VkDevice` |
| **Effect compute** | Mature. Vulkan Compute works on every Omarchy driver |
| **Video decode (Vulkan Video)** | Mature on AMD and Intel (H.264, H.265, AV1, VP9). FFmpeg 9 treats Vulkan as a complete backend |
| **Video encode (Vulkan Video)** | Works on AMD/Intel; **on NVIDIA, NVENC is much faster** |

The centerpiece of the design is **a single `VkDevice` shared** by FFmpeg (decode/encode),
OmaMovie's compositor and Qt Quick (preview). That way a frame is born, composited and displayed
without leaving VRAM.

---

## 2. Vulkan-Hpp

Khronos's official C++ bindings for Vulkan, shipped with the headers (`vulkan-headers` on Arch).

### 2.1 What it offers
- Strong types, `enum class`, typed flags, STL integration, no runtime CPU overhead.
- **`vk::raii`**: RAII wrappers for every Vulkan object (automatic destruction in the right order).
- A dynamic dispatcher (`VULKAN_HPP_DISPATCH_LOADER_DYNAMIC`) to load extension functions at runtime.
- A **C++20 module** named `vulkan` (renamed from `vulkan_hpp` in version 1.4.334), with
  `import std` support through `VULKAN_HPP_ENABLE_STD_MODULE`.

### 2.2 Exceptions vs. `std::expected`
- By default Vulkan-Hpp **throws exceptions**.
- `VULKAN_HPP_NO_EXCEPTIONS`: functions return `ResultValue<T>` (result + value).
- `VULKAN_HPP_RAII_NO_EXCEPTIONS`: `vk::raii` functions return `VULKAN_HPP_EXPECTED<T, vk::Result>`,
  which can be configured as `std::expected`.

**For OmaMovie:** `CLAUDE.md` §19 forbids exceptions crossing library boundaries and adopts
`std::expected`. Recommended configuration: `VULKAN_HPP_NO_EXCEPTIONS` +
`VULKAN_HPP_RAII_NO_EXCEPTIONS`, with `VULKAN_HPP_EXPECTED` = `std::expected`. Vulkan errors are
converted to `oma::Error` at the `libs/gpu` boundary.

### 2.3 Compile cost
- `vulkan.hpp` and `vulkan_raii.hpp` are huge headers; parsing and template codegen weigh on the build.
- Mitigations, in order of preference:
  1. **Confine the include** to `.cpp` files in `libs/gpu` and `libs/compositor`. The public
     headers of those libs expose OmaMovie's own types (or forward declarations), never
     `vulkan.hpp`. The dependency rule in `CLAUDE.md` §5.2 already requires that.
  2. **Precompiled header** in those two libs.
  3. **C++20 module `vulkan`**: viable with Clang 22/GCC 16, but modules still have friction
     with external headers. Evaluate in a spike before adopting.

### 2.4 Living with FFmpeg and Qt
FFmpeg and Qt use the C API (`VkDevice`, `VkImage`, ...). Vulkan-Hpp handles are compatible
through explicit conversion (`static_cast<VkImage>(image)` and back). Rule: **ownership stays with
the creator**. Objects received from FFmpeg or Qt are not wrapped in `vk::raii` (which would
destroy the object); use non-RAII handles (`vk::Image`) for borrowed objects.

### 2.5 Memory: Vulkan Memory Allocator (VMA)
- VMA (AMD GPUOpen) is the market standard for Vulkan memory allocation; it is header-only, but
  the implementation must be compiled in a `.cpp`. A C++ binding exists (`VulkanMemoryAllocator-Hpp`).
- **Imported memory (DMA-BUF, CUDA) does not go through VMA.** `libs/gpu` needs two paths: our
  own allocations through VMA and external imports with their own lifetime (`CLAUDE.md` §8.1).

---

## 3. Vulkan Video (decode and encode)

### 3.1 Support in the Mesa drivers

| Driver | Decode | Encode |
|---|---|---|
| **RADV** (AMD) | H.264, H.265 (since Mesa 23.1), AV1 (24.1), VP9 | H.264, H.265 (24.1), AV1 (25.2) |
| **ANV** (Intel) | H.264, H.265 (since 23.1), **AV1 (25.0, TigerLake onward, including 10-bit)**, VP9 (June 2025) | H.264, H.265 (24.3), AV1 on Arc/DG2 |

Mesa 26.0 brought general Vulkan Video improvements for H.264/H.265/AV1.

**Verified in S1 (`Docs/spikes/S1-hardware-inventory.md`):** on the Iris Xe (TigerLake) with Mesa
26.2.2, ANV exposes **no** Vulkan Video extensions by default; they appear only with
`ANV_DEBUG=video-decode,video-encode`, and AV1 decode still fails to initialize in FFmpeg 9.
VA-API is the default decode path on this generation.

### 3.2 FFmpeg and Vulkan
- **FFmpeg 7.1**: `h264_vulkan` and `hevc_vulkan` encoders; full decode → filter → encode pipelines in Vulkan.
- **FFmpeg 8.0**: `av1_vulkan`, VP9 decode in Vulkan and **compute-shader codecs** that run on
  any Vulkan 1.3 driver: FFv1 (encode/decode), ProRes RAW (decode); ProRes and VC-2 on the way.
  The announcement explicitly mentions non-linear editors as beneficiaries.
- **FFmpeg 9.0** (August 2026, the installed version): Vulkan as a complete backend for encode,
  decode and filters on all three vendors, through a single API.

### 3.3 NVIDIA
A report on the NVIDIA forum shows Vulkan Video encode **about 5x slower than NVENC** (~194 fps
vs. ~910 fps). On NVIDIA, prefer NVDEC/NVENC (see `cuda.md`).

### 3.4 Implications for OmaMovie
- **ProRes and FFv1 in compute shaders** enable GPU decode of professional intermediate codecs
  on any GPU, without a dedicated hardware block. Directly useful for proxies and intermediates
  (`CLAUDE.md` §15): an FFv1 or ProRes proxy decoded on the GPU keeps the whole pipeline in VRAM.
- Vulkan Video and VA-API coexist. The choice must be made **per codec and per driver, at
  runtime**, with benchmarks. Do not hardcode a single path.

---

## 4. VA-API → Vulkan (DMA-BUF)

The classic hardware decode path on Intel/AMD, and the most mature.

- VA-API exports the decoded surface as a **DMA-BUF**.
- Vulkan imports it with `VK_EXT_external_memory_dma_buf` + `VK_EXT_image_drm_format_modifier`,
  which describe the exact stride and tiling (a DRM modifier is a 64-bit integer defined in `drm_fourcc.h`).
- NV12 is multi-planar (`VK_FORMAT_G8_B8R8_2PLANE_420_UNORM`). A VA-API surface is usually a
  single allocation with two planes; the import must honor each plane's offsets.
- There are known incompatibilities between drivers: NVIDIA rejects certain NV12 layouts in the
  explicit-modifier path that it accepts in the list-based path.
- **FFmpeg already implements this mapping** (`av_hwframe_map` from VA-API to Vulkan, using the
  same extensions, enabled by default in `hwcontext_vulkan`). libplacebo and Dawn (Chrome) do too.

**Implication:** do not rewrite DMA-BUF import. Use FFmpeg's frame mapping on OmaMovie's
`VkDevice`, and only go down to the DMA-BUF level if a benchmark shows a problem.

---

## 5. FFmpeg `hwcontext_vulkan`

- `AVVulkanDeviceContext` describes the device. **The application can supply its own
  `VkInstance`/`VkDevice`/queues** instead of letting FFmpeg create them **(verify the exact
  fields in version 9)**. That is what makes the single device possible.
- Interop extensions enabled by default when available: `VK_KHR_external_memory_fd`,
  `VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`,
  `VK_KHR_external_semaphore_fd`, `VK_EXT_external_memory_host`.
- There is a queue family field for video decode.
- **`AVVkFrame`** carries **one timeline semaphore per `VkImage`** and its current value
  (`sem_value`). Contract: **wait** on that value in every submission that uses the image and
  **signal** the incremented value when done. The semaphore belongs to FFmpeg and must not be freed manually.
- Shared queues: Vulkan requires external synchronization of a `VkQueue`. `AVVulkanDeviceContext`
  has callbacks to lock/unlock queues **(verify names and availability in FFmpeg 9)**. OmaMovie
  needs one queue lock mechanism used by FFmpeg, the compositor and Qt.

**Implication:** the `AVVkFrame` synchronization contract (one timeline semaphore per image) is
the same model as `CLAUDE.md` §8.1. OmaMovie's `Frame` can wrap `AVVkFrame` while honoring that
contract, without copies.

---

## 6. Qt Quick on the same `VkDevice`

| API | Purpose |
|---|---|
| `QQuickGraphicsDevice::fromDeviceObjects(physicalDevice, device, queueFamilyIndex, queueIndex)` | Make Qt Quick **use OmaMovie's device**. It takes no ownership: OmaMovie guarantees the device outlives the window |
| `QNativeInterface::QSGVulkanTexture::fromNative(VkImage, VkImageLayout, window, size)` | Wrap the compositor's final image as a scene graph texture without a copy. **2D RGBA only**, called **on the scene graph render thread**, no ownership |
| `QQuickRenderControl` | Alternative: render the Qt scene into an offscreen target controlled by the application |
| "Scene Graph – Vulkan Texture Import" example | Official reference |

### Implications for OmaMovie
- The compositor output for the preview must be an **RGBA image** (e.g. RGBA16F or RGBA8 sRGB).
  YUV → RGB conversion happens in the compositor, not in Qt.
- Three actors share the same queue (Qt render thread, compositor, FFmpeg). Synchronization
  (semaphores, queue lock, layout transition of the image handed to Qt) is the project's
  **biggest technical risk**. It is the subject of ADR-0005 and must be validated in a spike before the UI.

---

## 7. libplacebo

A video rendering library on top of Vulkan, born in mpv and maintained by VideoLAN. The system
FFmpeg already links against it.

- High-quality scaling (polar/Jinc filters, anti-ringing, linear-light scaling).
- **Dynamic HDR tone mapping** (scene histogram, scene change detection, exposure control).
- **Colorimetrically accurate color management**: gamut mapping, ICC profiles, BT.1886, **3D `.cube` LUTs**.
- DMA-BUF import (used by mpv for VA-API interop).
- Backends: Vulkan, OpenGL, D3D11.

### Implications for OmaMovie
- libplacebo covers exactly the hardest parts of the compositor to get right: color
  conversion, tone mapping, quality scaling and LUTs (`CLAUDE.md` §7.4, ADR-0006).
- Question for the compositor ADR: **use libplacebo as a library of "color/scaling stages"
  inside the render graph** (operating on OmaMovie's `VkDevice`), or write our own shaders? For:
  proven quality and it is already a transitive FFmpeg dependency. Against: control of the render
  graph and of synchronization; we need to verify it accepts external devices and images without
  copies **(verify the `pl_vulkan_import` API)**. LGPL-2.1 license **(verify)**.

---

## 8. Recommendations

1. **`libs/gpu` owns the `VkInstance`/`VkDevice`**, created with the interop and video
   extensions and those Qt requires. FFmpeg and Qt receive that device.
2. **Vulkan-Hpp with `vk::raii` and no exceptions**, included only in `.cpp` files of `libs/gpu`/`libs/compositor`, with a PCH.
3. **VMA** for our allocations; a separate path for imported memory.
4. **Decode**: FFmpeg with the Vulkan or VA-API hwaccel, chosen per codec/driver at runtime; VA-API
   frames mapped to Vulkan by FFmpeg itself.
5. **Encode**: Vulkan or VA-API on AMD/Intel; NVENC on NVIDIA.
6. **Effects**: Vulkan Compute as the generic backend (OpenCL removed; see `opencl.md`).
7. **Mandatory spikes before committing to the design**:
   - a single device shared by FFmpeg + Qt + compositor, with a queue lock;
   - Vulkan Video vs. VA-API decode on the Iris Xe (H.264, HEVC, AV1), measuring frame time and CPU use;
   - libplacebo operating on OmaMovie's images without copies.

---

## Sources

- [Khronos: Vulkan-Hpp README](https://cdn.jsdelivr.net/gh/khronosgroup/vulkan-hpp@main/README.md)
- [NVIDIA: Vulkan C++ bindings reloaded](https://developer.nvidia.com/vulkan-c-bindings-reloaded)
- [NVIDIA: Preferring compile-time errors over runtime errors with Vulkan-Hpp](https://developer.nvidia.com/blog/preferring-compile-time-errors-over-runtime-errors-with-vulkan-hpp/)
- [vulkan_hpp_macros.hpp (SwiftShader mirror)](https://swiftshader.googlesource.com/SwiftShader.git/+/refs/heads/master/include/vulkan/vulkan_hpp_macros.hpp)
- [TU Wien: Auto-Vk report (compile time, modules, PCH)](https://www.cg.tuwien.ac.at/research/publications/2023/jafari-2023-avk/jafari-2023-avk-report.pdf)
- [Vulkan Memory Allocator: quick start](https://ctan.net/graphics/asymptote/VulkanMemoryAllocator/docs/html/quick_start.html)
- [VulkanMemoryAllocator (GPUOpen)](https://chromium.googlesource.com/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/+/refs/heads/master)
- [Phoronix: Mesa 23.1 RADV Vulkan Video decoding](https://www.phoronix.com/news/Mesa-23.1-RADV-Vulkan-Video)
- [Phoronix: RADV Vulkan Video H.264/H.265 encode](https://www.phoronix.com/news/RADV-Vulkan-VIdeo-H265-H264)
- [Phoronix: Mesa 25.2 RADV AV1 encode](https://www.phoronix.com/news/RADV-Merges-AV1-Encode)
- [Phoronix: Intel ANV merges initial AV1 decode](https://www.phoronix.com/news/Intel-Vulkan-Video-AV1-Decode)
- [Phoronix: Intel ANV fixes for AV1 decoding](https://www.phoronix.com/news/Intel-ANV-Fixing-AV1-Video)
- [Vulkan.org: Intel Vulkan driver merges H.264/H.265 encode](https://www.vulkan.org/news/auto-22797-58e532ef7d4ddf92a61f082073801d36)
- [Phoronix: Intel ANV AV1 encode for DG2](https://www.phoronix.com/news/Intel-DG2-Vulkan-Video-AV1)
- [Linux Adictos: Mesa 26.0 Vulkan improvements](https://en.linuxadictos.com/Mesa-26.0-strengthens-Vulkan-support-and-adds-dozens-of-key-extensions-to-radv--anv--nvk--panvk--Venus--and-other-drivers..html)
- [Vulkanised 2024: A Vulkan Video encoder from Mesa to GStreamer (Igalia)](https://vulkan.org/user/pages/09.events/vulkanised-2024/vulkanised-2024-stephane-cerveau-ko-igalia.pdf)
- [AlternativeTo: FFmpeg 7.1 released with Vulkan hardware encoding](https://alternativeto.net/news/2024/9/ffmpeg-7-1-released-with-stable-vvc-decoder-vulkan-hardware-encoding-and-much-more)
- [FFmpeg-devel: Announce FFmpeg 8.0](https://ffmpeg.org/pipermail/ffmpeg-devel/2025-August/347917.html)
- [OMG! Ubuntu: FFmpeg 8 Vulkan compute codecs](https://omgubuntu.co.uk/2025/08/ffmpeg-8-vulkan-compute-codecs-professional-video)
- [Rendi: FFmpeg 8.0 — attempts with Vulkan AV1 encoding / VP9 decoding](https://www.rendi.dev/blog/ffmpeg-8-0-part-3-failed-attempts-to-use-vulkan-for-av1-encoding-vp9-decoding)
- [FOSS Linux: Install FFmpeg with Vulkan hardware acceleration](https://www.fosslinux.com/159892/install-ffmpeg-vulkan-hardware-acceleration-linux.htm)
- [NVIDIA Forums: Vulkan Video is 5x slower than NVENC](https://forums.developer.nvidia.com/t/vulkan-video-is-5x-slower-than-nvenc/373916)
- [FFmpeg Doxygen: hwcontext_vulkan.h](https://ffmpeg.org/doxygen/6.0/hwcontext__vulkan_8h_source.html)
- [FFmpeg Doxygen: AVVkFrame](https://ffmpeg.org/doxygen/7.1/structAVVkFrame.html)
- [libplacebo: Vulkan dma_buf import MR](https://code.videolan.org/videolan/libplacebo/merge_requests/64)
- [Dawn: DMA-BUF memory service](https://dawn.googlesource.com/dawn.git/+/d368f2c2bff35d63b7c188485a02a6058356bb1e/src/dawn/native/vulkan/external_memory/MemoryServiceImplementationDmaBuf.cpp)
- [Collabora: Implementing DRM format modifiers in NVK](https://test.www.collabora.com/news-and-blog/news-and-events/implementing-drm-format-modifiers-in-nvk.html)
- [NVIDIA Forums: VkImageDrmFormatModifierExplicitCreateInfoEXT rejects NV12 layouts](https://forums.developer.nvidia.com/t/nvidia-vulkan-vkimagedrmformatmodifierexplicitcreateinfoext-rejects-nv12-layouts-that-the-list-based-path-accepts/371199)
- [Qt: QQuickGraphicsDevice](https://doc.qt.io/qt/qquickgraphicsdevice.html)
- [Qt: QNativeInterface::QSGVulkanTexture](https://doc.qt.io/qt/qnativeinterface-qsgvulkantexture.html)
- [Qt: QQuickRenderControl](https://doc.qt.io/qt-6.5/qquickrendercontrol.html)
- [libplacebo (GitHub mirror)](https://github.com/haasn/libplacebo)
- [VideoLAN: libplacebo project](https://www-test.videolan.org/projects/libplacebo/)
