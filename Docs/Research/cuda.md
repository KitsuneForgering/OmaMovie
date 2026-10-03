# CUDA e o caminho NVIDIA no OmaMovie

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Itens marcados **(verificar)** precisam de confirmação.
>
> Verificado localmente: `nvidia-utils` 610.57 fornece `libcuda.so`,
> `libnvcuvid.so` (NVDEC) e `libnvidia-encode.so` (NVENC). O FFmpeg do sistema
> foi compilado com `--enable-nvdec --enable-nvenc --enable-cuda-llvm`. O
> pacote `cuda` (toolkit) do Arch tem **4,71 GiB** e não vem instalado.

---

## 1. Resumo

"Usar CUDA" no contexto do OmaMovie são **três coisas diferentes**, com custos muito diferentes:

| O quê | Precisa do CUDA toolkit? | Ganho | Recomendação |
|---|---|---|---|
| **NVDEC/NVENC** (decode/encode por hardware) | **Não**. O FFmpeg carrega as bibliotecas do driver em runtime | Alto: NVENC é cerca de 5x mais rápido que Vulkan Video encode na NVIDIA | **Sim, desde cedo** |
| **Interop CUDA ↔ Vulkan** (levar o frame do NVDEC ao compositor Vulkan) | Não, usa a driver API (`libcuda`) | Necessário para manter o frame na VRAM | **Sim**, junto com NVDEC |
| **Kernels CUDA próprios** (efeitos) | Só no build (headers/libdevice); em runtime basta o driver | Depende do efeito; Vulkan Compute cobre a maioria | **Só com ganho medido** |

Conclusão: na NVIDIA, o maior ganho vem do NVDEC/NVENC, que **não exige
distribuir nem compilar com o toolkit**. Kernels CUDA são uma especialização posterior.

---

## 2. NVDEC e NVENC

- Parte do NVIDIA Video Codec SDK. O FFmpeg acessa via `ffnvcodec-headers`, que
  **carrega `libcuda`/`libnvcuvid`/`libnvidia-encode` dinamicamente**. Não há link
  em tempo de build contra bibliotecas da NVIDIA **(verificar: comportamento conhecido do ffnvcodec)**.
- O frame decodificado pelo NVDEC fica em **memória CUDA** (hwaccel `cuda` do FFmpeg).
- NVENC aceita frames CUDA diretamente.
- Comparação relatada no fórum da NVIDIA: encode via Vulkan Video ~194 fps contra NVENC ~910 fps.

### Omarchy e NVIDIA
O Omarchy instala `nvidia-open-dkms`, `nvidia-utils` e `libva-nvidia-driver`
(GPUs com GSP) e define `LIBVA_DRIVER_NAME=nvidia` e `NVD_BACKEND=direct`. Ou
seja, existe VA-API na NVIDIA, mas via uma camada de tradução sobre o NVDEC.

**Para o OmaMovie:** na NVIDIA, usar NVDEC/NVENC diretamente (hwaccel `cuda` do
FFmpeg), não a camada VA-API, que acrescenta uma tradução e tem limitações
conhecidas de DMA-BUF/NV12 na importação pelo Vulkan (ver `vulkan.md` §4).

---

## 3. Interop CUDA ↔ Vulkan

O compositor é Vulkan, mas o frame do NVDEC está em memória CUDA. Para não passar pela RAM:

1. O OmaMovie cria no Vulkan uma imagem/buffer **exportável** (`VK_KHR_external_memory_fd`, handle opaque fd).
2. A CUDA importa essa memória: `cuImportExternalMemory` (driver API) ou `cudaImportExternalMemory` (runtime API).
3. Copia-se a superfície do NVDEC para essa memória com uma **cópia device-to-device** (fica na VRAM; não é zero-copy, mas não há download).
4. Sincronização via **timeline semaphore** exportado do Vulkan e importado na CUDA
   (`cudaImportExternalSemaphore`, `cudaSignalExternalSemaphoresAsync`, `cudaWaitExternalSemaphoresAsync`).

**Restrição importante:** na CUDA é **ilegal esperar antes que o sinal
correspondente tenha sido emitido**. O "wait-before-signal" dos timeline
semaphores do Vulkan não vale do lado CUDA. O agendador do OmaMovie precisa
garantir a ordem (sinal submetido antes da espera ser submetida na CUDA).

Alternativas a avaliar:
- **Mapeamento de frames do FFmpeg** entre os contextos `cuda` e `vulkan`
  **(verificar se o FFmpeg 9 suporta `av_hwframe_map`/transfer CUDA→Vulkan sem passar pela CPU)**. Se suportar, evita escrever o interop à mão.
- **Vulkan Video decode direto na NVIDIA**: elimina o interop. O desempenho de
  decode (não o de encode) precisa ser medido antes de descartar.

---

## 4. Kernels CUDA próprios

### Quando valeria
- Efeitos pesados em que CUDA tenha vantagem **medida** sobre Vulkan Compute na
  mesma GPU (ex.: bibliotecas maduras como NPP, ou kernels que dependem de recursos específicos).
- O `CLAUDE.md` já diz: CUDA só onde houver ganho real, e efeitos não dependem diretamente de CUDA.

### Como compilar sem `nvcc` e sem exigir o toolkit em runtime
O FFmpeg do sistema usa `--enable-cuda-llvm`: os kernels CUDA dele são
compilados pelo **clang para PTX**. O mesmo padrão serve ao OmaMovie:

1. Kernels `.cu` compilados com **clang** para PTX (o LLVM suporta CUDA desde a
   3.9). O build precisa dos headers/libdevice do CUDA (`--cuda-path`).
2. O PTX é **embutido no binário**.
3. Em runtime, carrega-se `libcuda.so` via `dlopen` (vem com `nvidia-utils`) e o
   PTX via driver API (`cuModuleLoadData` / `cuLinkAddData`). O driver faz o JIT para a GPU presente.

Consequências:
- Usuário final **não precisa** instalar o toolkit (4,7 GiB).
- Máquinas sem NVIDIA não carregam nada; o backend CUDA simplesmente não aparece.
- O build com CUDA fica **opcional** (opção CMake), para não exigir o toolkit de todos os desenvolvedores.

---

## 5. Notebooks híbridos (Intel/AMD + NVIDIA)

O Omarchy detecta GPUs híbridas e evita acordar a dGPU à toa (os detectores
leem sysfs em vez de `lspci`, porque acordar a GPU estoura o tempo de reload do Hyprland).

**Implicações para o OmaMovie:**
- Escolha de device é uma decisão de primeira classe: decode na dGPU e
  composição/apresentação na iGPU implica **cópia entre GPUs** pelo barramento.
- Regra inicial proposta: **um device primário para todo o pipeline** (decode,
  composição, preview), escolhido pelo usuário ou por heurística (dGPU quando ligada à tomada?), registrado em log.
- Não acordar a dGPU só para enumerar capacidades; usar informações do sysfs/Vulkan sem criar device quando possível **(verificar)**.

---

## 6. Licenciamento

- Carregar `libcuda`/`libnvcuvid`/`libnvidia-encode` dinamicamente a partir do
  driver instalado é o modelo usado pelo FFmpeg e outros projetos livres.
- O toolkit CUDA tem EULA própria. Não redistribuir partes do toolkit; o PTX
  gerado pelo OmaMovie é código do projeto **(verificar a EULA para libdevice embutida no PTX)**.

---

## 7. Recomendações

1. **Fase de decode/encode (M1–M6)**: NVDEC/NVENC via FFmpeg na NVIDIA, sem toolkit.
2. **Interop**: tentar primeiro o mapeamento de frames do FFmpeg; se não houver
   caminho sem CPU, implementar o interop de memória externa + timeline semaphore em `libs/gpu`.
3. **Benchmark**: NVDEC + interop vs. Vulkan Video decode direto na NVIDIA. Escolher por medição.
4. **`CudaBackend` de compute**: só depois que o `ComputeBackend` com Vulkan existir e houver um efeito com ganho medido. PTX via clang, `dlopen` em runtime, build opcional.
5. **Hardware de teste**: a máquina atual não tem GPU NVIDIA (`nvidia-utils` está
   instalado, mas o `lspci` só mostra a Iris Xe). Validar o caminho NVIDIA exige outra máquina.

---

## Fontes

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
- Arquivos locais do Omarchy 4.0.4: `/usr/share/omarchy/install/hardware/nvidia.sh`, `/usr/share/omarchy/default/hypr/nvidia.lua`
