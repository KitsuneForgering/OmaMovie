# Vulkan e Vulkan-Hpp no OmaMovie

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Itens marcados **(verificar)** são inferências ou
> conhecimento geral que precisam ser confirmados em código ou documentação
> antes de virar decisão.
>
> Ambiente verificado localmente: Mesa 26.2.2 (`vulkan-intel`), Vulkan loader
> 1.4.357, FFmpeg 9.0.1 (com `--enable-vulkan --enable-libplacebo`), Qt 6.11.2,
> GPU Intel Iris Xe (TigerLake). Não instalados: `vulkan-headers` (onde está o
> `vulkan.hpp`), `vulkan-tools`.

---

## 1. Resumo

O Vulkan é a **espinha dorsal** do OmaMovie. Ele serve a quatro papéis
diferentes, e para dois deles o ecossistema amadureceu bastante em 2025–2026:

| Papel | Situação |
|---|---|
| **Composição e preview** | Maduro. Qt Quick roda sobre Vulkan e aceita um `VkDevice` externo |
| **Compute de efeitos** | Maduro. Vulkan Compute funciona em todos os drivers do Omarchy |
| **Decode de vídeo (Vulkan Video)** | Maduro em AMD e Intel (H.264, H.265, AV1, VP9). FFmpeg 9 trata o Vulkan como backend completo |
| **Encode de vídeo (Vulkan Video)** | Funciona em AMD/Intel; **na NVIDIA o NVENC é muito mais rápido** |

A peça central do desenho é **um único `VkDevice` compartilhado** entre FFmpeg
(decode/encode), o compositor do OmaMovie e o Qt Quick (preview). Assim o
frame nasce, é composto e é exibido sem sair da VRAM.

---

## 2. Vulkan-Hpp

Bindings C++ oficiais da Khronos para Vulkan, distribuídos junto com os headers (`vulkan-headers` no Arch).

### 2.1 O que oferece
- Tipos fortes, `enum class`, flags tipadas, integração com STL, sem overhead de CPU em runtime.
- **`vk::raii`**: wrappers RAII para todos os objetos Vulkan (destruição automática na ordem certa).
- Dispatcher dinâmico (`VULKAN_HPP_DISPATCH_LOADER_DYNAMIC`) para carregar funções de extensões em runtime.
- **Módulo C++20** chamado `vulkan` (renomeado de `vulkan_hpp` na versão 1.4.334), com suporte a `import std` via `VULKAN_HPP_ENABLE_STD_MODULE`.

### 2.2 Exceções vs. `std::expected`
- Por padrão o Vulkan-Hpp **lança exceções**.
- `VULKAN_HPP_NO_EXCEPTIONS`: as funções retornam `ResultValue<T>` (resultado + valor).
- `VULKAN_HPP_RAII_NO_EXCEPTIONS`: as funções de `vk::raii` retornam
  `VULKAN_HPP_EXPECTED<T, vk::Result>`, que pode ser configurado como `std::expected`.

**Para o OmaMovie:** o `CLAUDE.md` §19 proíbe exceções atravessando fronteiras
de libs e adota `std::expected`. Configuração recomendada:
`VULKAN_HPP_NO_EXCEPTIONS` + `VULKAN_HPP_RAII_NO_EXCEPTIONS`, com `VULKAN_HPP_EXPECTED` = `std::expected`.
Erros de Vulkan são convertidos em `oma::Error` na borda de `libs/gpu`.

### 2.3 Custo de compilação
- `vulkan.hpp` e `vulkan_raii.hpp` são headers enormes; o parse e a codegen de templates pesam no build.
- Mitigações, em ordem de preferência:
  1. **Confinar o include** a arquivos `.cpp` de `libs/gpu` e `libs/compositor`.
     Headers públicos dessas libs expõem tipos próprios do OmaMovie (ou forward
     declarations), nunca `vulkan.hpp`. Isso já é exigido pela regra de dependências do `CLAUDE.md` §5.2.
  2. **Precompiled header** (`target_precompile_headers`) nessas duas libs.
  3. **Módulo C++20 `vulkan`**: viável com CMake 4.4 + Clang 22/GCC 16, mas
     módulos ainda têm atrito com headers externos. Avaliar num spike antes de adotar.

### 2.4 Convivência com FFmpeg e Qt
FFmpeg e Qt usam a API C (`VkDevice`, `VkImage`, ...). Os handles do Vulkan-Hpp
são compatíveis por conversão explícita (`static_cast<VkImage>(image)` e
vice-versa). Regra: **ownership fica com quem criou**. Objetos recebidos do
FFmpeg ou do Qt não são embrulhados em `vk::raii` (que destruiria o objeto);
usar handles não-RAII (`vk::Image`) para objetos emprestados.

### 2.5 Memória: Vulkan Memory Allocator (VMA)
- VMA (AMD GPUOpen) é o padrão de mercado para alocação de memória Vulkan;
  é header-only, mas a implementação deve ser compilada num `.cpp`. Existe um binding C++ (`VulkanMemoryAllocator-Hpp`).
- **Memória importada (DMA-BUF, CUDA) não passa pelo VMA.** O `libs/gpu`
  precisa de dois caminhos: alocações próprias via VMA e importações externas com lifetime próprio (`CLAUDE.md` §8.1).

---

## 3. Vulkan Video (decode e encode)

### 3.1 Suporte nos drivers do Mesa

| Driver | Decode | Encode |
|---|---|---|
| **RADV** (AMD) | H.264, H.265 (desde Mesa 23.1), AV1 (24.1), VP9 | H.264, H.265 (24.1), AV1 (25.2) |
| **ANV** (Intel) | H.264, H.265 (desde 23.1), **AV1 (25.0, TigerLake em diante, inclusive 10-bit)**, VP9 (junho de 2025) | H.264, H.265 (24.3), AV1 em Arc/DG2 |

Mesa 26.0 trouxe melhorias gerais de Vulkan Video para H.264/H.265/AV1. A
máquina de desenvolvimento (Iris Xe, TigerLake) deve suportar decode de AV1 via Vulkan **(verificar com `vulkaninfo`)**.

### 3.2 FFmpeg e Vulkan
- **FFmpeg 7.1**: encoders `h264_vulkan` e `hevc_vulkan`; pipelines inteiros decode → filtro → encode em Vulkan.
- **FFmpeg 8.0**: `av1_vulkan`, decode VP9 em Vulkan e **codecs em compute
  shader** que rodam em qualquer driver Vulkan 1.3: FFv1 (encode/decode),
  ProRes RAW (decode); ProRes e VC-2 a caminho. O anúncio cita explicitamente editores não lineares como beneficiados.
- **FFmpeg 9.0** (agosto de 2026, é a versão instalada): Vulkan como backend
  completo para encode, decode e filtros, nos três fabricantes, por uma API única.

### 3.3 NVIDIA
Há relato no fórum da NVIDIA de encode via Vulkan Video **cerca de 5x mais
lento que NVENC** (~194 fps contra ~910 fps). Na NVIDIA, preferir NVDEC/NVENC (ver `cuda.md`).

### 3.4 Implicações para o OmaMovie
- **ProRes e FFv1 em compute shader** permitem decode GPU de codecs
  intermediários profissionais em qualquer GPU, sem bloco de hardware dedicado.
  Isso é diretamente útil para proxies e intermediários (`CLAUDE.md` §15): um
  proxy FFv1 ou ProRes decodificado na GPU mantém o pipeline todo na VRAM.
- Vulkan Video e VA-API coexistem. A escolha deve ser **por codec e por
  driver, em runtime**, com benchmark. Não fixar um caminho único.

---

## 4. VA-API → Vulkan (DMA-BUF)

Caminho clássico de decode por hardware em Intel/AMD, e o mais maduro.

- O VA-API exporta a superfície decodificada como **DMA-BUF**.
- O Vulkan importa com `VK_EXT_external_memory_dma_buf` +
  `VK_EXT_image_drm_format_modifier`, que descrevem stride e tiling exatos (o
  modificador DRM é um inteiro de 64 bits definido em `drm_fourcc.h`).
- NV12 é multi-planar (`VK_FORMAT_G8_B8R8_2PLANE_420_UNORM`). A superfície VA-API
  costuma ser uma única alocação com dois planos; a importação precisa respeitar os offsets de cada plano.
- Há incompatibilidades conhecidas entre drivers: a NVIDIA rejeita certos
  layouts NV12 no caminho explícito de modificadores que aceita no caminho por lista.
- **O FFmpeg já implementa esse mapeamento** (`av_hwframe_map` de VA-API para
  Vulkan, usando as mesmas extensões, habilitadas por padrão no `hwcontext_vulkan`). libplacebo e Dawn (Chrome) também.

**Implicação:** não reescrever a importação de DMA-BUF. Usar o mapeamento de
frames do FFmpeg sobre o `VkDevice` do OmaMovie, e só descer ao nível do
DMA-BUF se um benchmark mostrar problema.

---

## 5. FFmpeg `hwcontext_vulkan`

- `AVVulkanDeviceContext` descreve o device. **A aplicação pode fornecer o
  próprio `VkInstance`/`VkDevice`/filas** em vez de deixar o FFmpeg criar
  **(verificar os campos exatos na versão 9)**. É isso que permite o device único.
- Extensões de interop habilitadas por padrão quando disponíveis:
  `VK_KHR_external_memory_fd`, `VK_EXT_external_memory_dma_buf`,
  `VK_EXT_image_drm_format_modifier`, `VK_KHR_external_semaphore_fd`, `VK_EXT_external_memory_host`.
- Há um campo de família de fila para decode de vídeo.
- **`AVVkFrame`** carrega **um timeline semaphore por `VkImage`** e o valor atual
  (`sem_value`). Contrato: **esperar** nesse valor em toda submissão que usa a
  imagem e **sinalizar** o valor incrementado ao terminar. O semáforo pertence ao FFmpeg e não deve ser liberado manualmente.
- Fila compartilhada: Vulkan exige sincronização externa de `VkQueue`. O
  `AVVulkanDeviceContext` tem callbacks para travar/destravar filas **(verificar
  nomes e disponibilidade no FFmpeg 9)**. O OmaMovie precisa de um único mecanismo
  de lock de fila usado por FFmpeg, compositor e Qt.

**Implicação:** o contrato de sincronização do `AVVkFrame` (timeline semaphore
por imagem) é o mesmo modelo do `CLAUDE.md` §8.1. O `Frame` do OmaMovie pode
embrulhar o `AVVkFrame` respeitando esse contrato, sem cópia.

---

## 6. Qt Quick sobre o mesmo `VkDevice`

| API | Para quê |
|---|---|
| `QQuickGraphicsDevice::fromDeviceObjects(physicalDevice, device, queueFamilyIndex, queueIndex)` | Fazer o Qt Quick **usar o device do OmaMovie**. Não assume ownership: o OmaMovie garante que o device vive mais que a janela |
| `QNativeInterface::QSGVulkanTexture::fromNative(VkImage, VkImageLayout, window, size)` | Embrulhar a imagem final do compositor como textura do scene graph, sem cópia. **Só 2D RGBA**, chamada **na thread de render do scene graph**, sem ownership |
| `QQuickRenderControl` | Alternativa: renderizar a cena Qt num alvo offscreen controlado pela aplicação |
| Exemplo "Scene Graph – Vulkan Texture Import" | Referência oficial |

### Implicações para o OmaMovie
- A saída do compositor para o preview deve ser uma **imagem RGBA** (ex.:
  RGBA16F ou RGBA8 sRGB). A conversão YUV → RGB acontece no compositor, não no Qt.
- Há três atores na mesma fila (Qt render thread, compositor, FFmpeg). A
  sincronização (semáforos, lock de fila, transição de layout da imagem
  entregue ao Qt) é o **maior risco técnico** do projeto. É o tema do ADR-0005 e deve ser validado num spike antes da UI.

---

## 7. libplacebo

Biblioteca de renderização de vídeo sobre Vulkan, nascida no mpv e mantida pela VideoLAN. O FFmpeg do sistema já linka com ela.

- Escala de alta qualidade (filtros polares/Jinc, anti-ringing, escala em luz linear).
- **Tone mapping HDR dinâmico** (histograma de cena, detecção de troca de cena, controle de exposição).
- **Gerenciamento de cor colorimetricamente preciso**: gamut mapping, perfis ICC, BT.1886, **LUTs 3D `.cube`**.
- Importação de DMA-BUF (usada pelo mpv para interop com VA-API).
- Backends: Vulkan, OpenGL, D3D11.

### Implicações para o OmaMovie
- libplacebo cobre exatamente as partes do compositor mais difíceis de acertar:
  conversão de cor, tone mapping, escala de qualidade e LUTs (`CLAUDE.md` §7.4, ADR-0006).
- Pergunta para o ADR do compositor: **usar libplacebo como biblioteca de
  "estágios de cor/escala" dentro do render graph** (operando sobre o `VkDevice` do
  OmaMovie), ou implementar shaders próprios? Ponto a favor: qualidade
  comprovada e já é dependência transitiva do FFmpeg. Ponto contra: controle do
  render graph e da sincronização; é preciso verificar se aceita device e
  imagens externas sem cópia **(verificar a API `pl_vulkan_import`)**. Licença LGPL-2.1 **(verificar)**.

---

## 8. Recomendações

1. **`libs/gpu` é dono do `VkInstance`/`VkDevice`**, criados com as extensões de
   interop, vídeo e as exigidas pelo Qt. FFmpeg e Qt recebem esse device.
2. **Vulkan-Hpp com `vk::raii` e sem exceções**, incluído só em `.cpp` de `libs/gpu`/`libs/compositor`, com PCH.
3. **VMA** para alocações próprias; caminho separado para memória importada.
4. **Decode**: FFmpeg com hwaccel Vulkan ou VA-API, escolhido por codec/driver em
   runtime; frames VA-API mapeados para Vulkan pelo próprio FFmpeg.
5. **Encode**: Vulkan ou VA-API em AMD/Intel; NVENC na NVIDIA.
6. **Efeitos**: Vulkan Compute como backend genérico (OpenCL removido; ver `opencl.md`).
7. **Spikes obrigatórios antes de fixar o desenho**:
   - device único compartilhado por FFmpeg + Qt + compositor, com lock de fila;
   - decode Vulkan Video vs. VA-API na Iris Xe (H.264, HEVC, AV1), medindo frame time e uso de CPU;
   - libplacebo operando sobre imagens do OmaMovie sem cópia.

---

## Fontes

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
