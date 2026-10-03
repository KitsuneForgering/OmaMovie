# Source register — skeptical review, 2026-10-03

All entries were accessed on **2026-10-03**. Online access was the browser's extracted
HTML/text or PDF text, at the passages listed; linked subpages were not implicitly read.
Unknown publication dates and unpinned live revisions remain **unknown**. `latest`, `trunk`
and `master` are moving references, not version locks. Vendor product/help pages support
feature descriptions, not independent performance or user preference evidence.

| ID | Author / institution; direct source | Date / version actually exposed | Passage inspected; role and limits |
|---|---|---|---|
| F1 | FFmpeg developers, [release news](https://ffmpeg.org/index.html) | 7.1: 2024-09-30; 8.0: 2025-08-22; 8.1: 2026-03-16 | Release highlights for Vulkan/compute codecs; accessed page did not establish a 9.0 release |
| F2 | FFmpeg developers, [Vulkan context header](https://ffmpeg.org/doxygen/trunk/hwcontext__vulkan_8h_source.html) | trunk, exact commit unknown | `AVVulkanDeviceContext` queue hooks/flags and `AVVkFrame` synchronization; compared with installed header, not all mapping internals |
| F3 | FFmpeg developers, [nv-codec-headers](https://github.com/FFmpeg/nv-codec-headers) | live repository, revision unknown | README/SDK interface context; no toolkit-free build reproduction |
| F4 | FFmpeg developers, [legal considerations](https://ffmpeg.org/legal.html) | live page, update unknown | License/configuration distinctions; documentary engineering input, no jurisdiction-specific legal determination |
| K1 | Khronos, [vkGetDeviceQueue](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceQueue.html) | latest spec, revision unknown | Description and VUID flags-01841; normative restriction for queue retrieval |
| K2 | Khronos, [internally synchronized queues](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_internally_synchronized_queues.html) | extension revision 1; modified 2025-02-04 | Description, opt-in feature/flag; no automatic image synchronization |
| K3 | Khronos, [video capability query](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetPhysicalDeviceVideoCapabilitiesKHR.html) | latest spec, revision unknown | Profile-specific capabilities and codec-specific structures; not empirical driver support |
| K4 | Khronos, [OpenCL/Vulkan sample](https://docs.vulkan.org/samples/latest/samples/extensions/open_cl_interop/README.html) | latest sample, update unknown | Memory/semaphore extensions and UUID matching; feasibility example, not all runtime/image support |
| K5 | Khronos, [Vulkan-Hpp](https://github.com/KhronosGroup/Vulkan-Hpp) | live README, revision unknown | C++ bindings/RAII overview; installed macros/build are separate evidence |
| Q1 | Qt Company, [QQuickGraphicsDevice](https://doc.qt.io/qt-6/qquickgraphicsdevice.html) | online Qt 6.12.0; update unknown | `fromDeviceObjects` and borrowed-resource lifetime; version differs from local 6.11.2 |
| Q2 | Qt Company, [QSGVulkanTexture](https://doc.qt.io/qt-6/qnativeinterface-qsgvulkantexture.html) | online Qt 6.12.0; update unknown | `fromNative`: thread, RGBA and ownership contract; readiness handoff not provided by signature |
| Q3 | Qt Company, [qrhivulkan.cpp](https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/gui/rhi/qrhivulkan.cpp) | tag v6.11.2; commit hash not resolved | Imported-device branch and queue retrieval, lines 832–862; exact tagged upstream source, local binary/patch match not established |
| N1 | NVIDIA, [FFmpeg acceleration guide](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/ffmpeg-with-nvidia-gpu/index.html) | Video Codec SDK 13.0; date unknown | Prerequisites, build example, CUDA-frame transcode; vendor recipe includes toolkit/NPP, not minimal build proof |
| N2 | NVIDIA, [external resource API](https://docs.nvidia.com/cuda/cuda-runtime-api/cuda_runtime_api/group__CUDART__EXTRES__INTEROP.html) | CUDA Runtime 13.4; date unknown | Memory import and handle-specific semaphore wait semantics; no NVIDIA experiment performed |
| M1 | Mesa project, [Rusticl](https://docs.mesa3d.org/rusticl.html) | latest docs; update unknown | Enabling and build-time defaults; does not establish installed-runtime interop extension matrix |
| P1 | libplacebo maintainers, [Vulkan header](https://raw.githubusercontent.com/haasn/libplacebo/master/src/include/libplacebo/vulkan.h) | master, commit unknown | `pl_vulkan_import`, wrap params, lock callbacks; integration candidate, installed ABI not pinned |
| P2 | libplacebo maintainers, [introduction](https://libplacebo.org/) | live page, update unknown | Library role only; no quality/performance measurement inferred |
| A1 | Apple, [iMovie Mac guide](https://support.apple.com/en-nz/guide/imovie/welcome/mac) | live Mac guide, edition/date unknown | Illustrated overview and table of interactions; feature discovery, no underlying-engine proof |
| A2 | Apple, [send projects to Final Cut](https://support.apple.com/guide/imovie/send-projects-to-final-cut-pro-movcbf7e2a3f/mac) | live Mac guide, edition/date unknown | Project/media copying, trailer conversion, Gain mapping; one bounded workflow |
| D1 | Adobe, [color overview](https://helpx.adobe.com/premiere/desktop/correct-color/set-up-color-management/about-color-management.html) | updated 2026-01-07 | Input/working/output stages, presets and overrides; inspected color passage and guide index, not every indexed feature |
| B1 | Blackmagic Design, [Resolve tech specs](https://www.blackmagicdesign.com/products/davinciresolve/techspecs) | product identifies Resolve 21; update unknown | Product identity/page navigation and feature context; no 21.1/API-count verification |
| B2 | Blackmagic Design, [supported codecs PDF](https://documents.blackmagicdesign.com/SupportNotes/DaVinci_Resolve_20_Supported_Codec_List.pdf) | Resolve 20, July 2025 | Printed pp. 11/14, H.264/H.265/AAC Linux rows; parsed text, not installation test; no transfer to v21 assumed |
| T1 | OTIO project, [file format specification](https://opentimelineio.readthedocs.io/en/latest/tutorials/otio-file-format-specification.html) | site 0.19.0.dev1; embedded draft says Beta 13 | Version note, JSON numeric types/schema; recommends library; historical draft within moving docs, not current comprehensive schema |
| C1 | CapCut, [editing guide](https://www.capcut.com/resource/how-to-use-capcut) | live page, version/date unknown | Keyframes and Speed > Curve paragraphs; promotional description, no accuracy or popularity result |
| C2 | CapCut, [caption generator](https://www.capcut.com/tools/auto-caption-generator) | live page, version/date unknown | Generate/edit/style steps; promotional, no word-error-rate or regional-plan verification |
| W1 | Microsoft, [Clipchamp timeline](https://support.microsoft.com/en-us/clipchamp/how-to-work-with-the-timeline-in-clipchamp) | live help, version/date unknown | Add/reorder/trim/properties interactions; no usability comparison |
| E1 | Farid Abdelnour / Kdenlive, [State of Kdenlive](https://kdenlive.org/news/2026/state-2026/) | 2026-04-18 | Stability and planned MLT features; dated developer roadmap, not October availability audit |
| E2 | Shotcut project, [FAQ](https://www.shotcut.org/FAQ/) | live FAQ, version/update unknown | GPU processing modes, decode limitations and transfer costs; implementation description, not cross-editor benchmark |
| E3 | Descript, [video editing](https://www.descript.com/video-editing) | live product page, date/version unknown | Advertised text-based editing; no adoption/learnability measurement |
| R1 | PipeWire project, [Streams](https://docs.pipewire.org/page_streams.html) | generated 1.16.1; update unknown | Streaming, RT process flag and timing; RT safety contract, not local A/V validation |
| S1 | Linux man-pages project, [fsync(2)](https://man7.org/linux/man-pages/man2/fsync.2.html) | man-pages 6.19, 2026-02-08 | Description: file vs directory durability; local-filesystem design input |
| S2 | SQLite project, [atomic commit](https://www.sqlite.org/atomiccommit.html) | live page, update unknown | Transaction/durability discussion; counterexample to blanket database unreliability |

## Local documentary sources

[Environment snapshot and hashes](evidence/2026-10-03-local.txt) record FFmpeg, Qt, Mesa,
Omarchy package labels, the different installed Omarchy version-file label and inspected
script/header identities. They are observations of this machine, not upstream provenance.

- `hwcontext_vulkan.h`: queue fields, semaphore values and locks (installed libavutil 61.1.101).
- Omarchy scripts: theme resolution/swap, capture directory/encoder intent, GPU package
  selection; Lua window rules and base package list. Source inspected, installer not run.
- Repository baseline `2f6abf3`: `libs/gpu/src/device.cpp`,
  `libs/media/src/vulkan_device.cpp`, `libs/compositor/src/color.cpp`,
  `tests/compositor/test_compositors.cpp`, `tests/support/lsan.supp`, public APIs/file layout.
  Code inspection is distinct from passing tests and performance measurement. The final
  existing debug-suite run is retained separately in [test output](evidence/2026-10-03-make-test.txt);
  it does not complete the proposed pilots.
- S1/S2 and implementation-plan timing paragraphs: previous experiment reports; not rerun.
  Procedures exist, but raw output, repetitions and provenance are incomplete in the docs.

## Failed access and exclusions

Apple Final Cut magnetic-timeline URLs attempted in this review, the CapCut `/resource/speed-curve`
URL and a Microsoft Windows Essentials URL returned errors. They supply no evidence.
CapCut's general guide provided a separate accessible speed-curve passage.

The original NVIDIA speed-ratio/forum, CapCut policy/pricing commentaries, review aggregators,
Movie Maker/iMovie histories and speculative future release details were not adopted as
verified evidence. This register does not imply every old link was checked or every product
installed. Vendor/help sources share their organization's lineage. No independent comparative
benchmark or representative user study was performed or found decisive in this bounded audit.
