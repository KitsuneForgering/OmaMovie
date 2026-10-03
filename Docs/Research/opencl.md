# OpenCL no OmaMovie: avaliação

> **Decisão (2026-10-02): OpenCL removido do projeto.** O Vulkan Compute é o
> backend genérico; CUDA continua como especialização NVIDIA. Este documento fica
> como registro da análise que motivou a decisão (`CLAUDE.md` §9.3).

> Pesquisa feita em 2026-10-02. Fatos com fonte estão linkados na seção
> [Fontes](#fontes). Itens marcados **(verificar)** precisam de confirmação.
>
> Verificado localmente: `ocl-icd` (loader) instalado, mas **nenhum ICD
> registrado** (`/etc/OpenCL/vendors` não existe). O instalador do Omarchy 4.0.4
> **não instala runtime OpenCL** para nenhum fabricante. Pacotes disponíveis no
> Arch: `opencl-mesa` (rusticl), `intel-compute-runtime`, `rocm-opencl-runtime`, `opencl-nvidia`.

---

## 1. Resumo

O `CLAUDE.md` descreve o OpenCL como "backend genérico de computação". A
pesquisa indica que, **no Omarchy e para um pipeline GPU-first, o Vulkan Compute
cumpre melhor esse papel**, e o OpenCL deveria ser um backend **opcional e
posterior**, condicionado a dois fatores:

1. **Interop sem cópia com Vulkan.** É o ponto decisivo. Em rusticl (Mesa) o
   `cl_khr_external_memory` ainda estava em desenvolvimento no fim de 2025. Sem
   ele, um efeito OpenCL obriga a copiar o frame, o que viola o princípio central do `CLAUDE.md` §7.
2. **Disponibilidade.** O runtime OpenCL não vem instalado no Omarchy. O Vulkan vem sempre.

Isso **diverge do `CLAUDE.md` atual** e precisa de decisão sua (ver §6).

---

## 2. Estado do OpenCL no Linux (2026)

| Runtime | Hardware | Situação |
|---|---|---|
| **rusticl** (Mesa, `opencl-mesa`) | Intel (iris), AMD (radeonsi), Zink (qualquer Vulkan), Asahi, llvmpipe | **OpenCL 3.1** no Mesa 26.2. Habilitado por padrão em alguns drivers (radeonsi, asahi, freedreno, zink); em outros exige `RUSTICL_ENABLE` |
| **intel-compute-runtime** (NEO) | Intel | Runtime oficial da Intel (OpenCL + Level Zero) |
| **ROCm** (`rocm-opencl-runtime`) | AMD | Runtime oficial da AMD |
| **NVIDIA** (`opencl-nvidia`, ~103 MiB) | NVIDIA | Suporta as extensões de interop com Vulkan |

### Interop com Vulkan
- Extensões Khronos: `cl_khr_external_memory` (+ `_opaque_fd`, `_dma_buf`),
  `cl_khr_semaphore`, `cl_khr_external_semaphore` (+ `_opaque_fd`). Elas
  permitem compartilhar memória e sincronizar com Vulkan.
- **NVIDIA** documenta o uso dessas extensões com o próprio OpenCL.
- **rusticl**: `cl_khr_external_memory` listado como trabalho em andamento na
  apresentação da XDC 2025. **(verificar o estado no Mesa 26.2)**.
- **intel-compute-runtime**: suporte às extensões externas não confirmado nesta pesquisa **(verificar)**.
- Há um sample oficial de Vulkan "Cross vendor OpenCL and Vulkan interoperability".

---

## 3. A lição do Blender

O Blender **removeu o OpenCL do Cycles na versão 3.0** (reescrita Cycles-X). Motivos citados:
- implementação limitada do kernel em OpenCL;
- **bugs de driver** de certos fabricantes;
- **padrão estagnado**;
- manutenção difícil de um código separado do caminho C++/CUDA.

Depois disso, AMD, Apple e Intel contribuíram backends **HIP, Metal e oneAPI**.

**Lição para o OmaMovie:** manter um backend de compute a mais custa caro e
depende da qualidade dos drivers. O Blender preferiu backends nativos por
fabricante a um backend "genérico" que funcionava mal em todos.

Contraponto: o OpenCL evoluiu desde 2021 (OpenCL 3.0/3.1, rusticl
maduro), e o Darktable continua usando OpenCL com sucesso **(conhecimento geral)**.

---

## 4. Comparação: Vulkan Compute vs. OpenCL para o OmaMovie

| Critério | Vulkan Compute | OpenCL |
|---|---|---|
| Disponível no Omarchy por padrão | **Sim** (drivers Vulkan sempre instalados) | Não |
| Acesso direto às imagens do compositor | **Sim, mesmo device e memória** | Só com extensões de interop |
| Interop sem cópia em Mesa | Não se aplica (nativo) | Em desenvolvimento (rusticl) |
| Linguagem de kernel | GLSL/HLSL/Slang → SPIR-V | OpenCL C / C++ for OpenCL → SPIR-V |
| Conveniência para computação geral | Menor (mais verboso, descriptors) | **Maior** (modelo de buffers e kernels mais simples) |
| Precisão e recursos numéricos | Depende de extensões | Bom suporte a precisão e builtins matemáticos |
| Ecossistema de kernels prontos | Shaders de vídeo (libplacebo, FFmpeg) | Bibliotecas científicas, Darktable, filtros OpenCL do FFmpeg |
| Uso no FFmpeg | Filtros Vulkan + decode/encode | Filtros OpenCL (`hwcontext_opencl`, com interop VA-API na Intel) |

---

## 5. Onde o OpenCL ainda pode fazer sentido

- **Reaproveitar kernels ou bibliotecas existentes** escritos em OpenCL, quando reescrever em Vulkan não compensar.
- **Computação não visual** (análise de áudio, detecção de cena, estatísticas)
  em que a simplicidade do modelo OpenCL ajuda e o dado não precisa estar numa imagem Vulkan.
- **NVIDIA**, se um efeito for mais simples em OpenCL do que em CUDA e o interop estiver disponível.

Nenhum desses casos existe no OmaMovie hoje.

---

## 6. Recomendação (para decisão)

1. **Vulkan Compute é o backend genérico padrão** de `ComputeBackend`. Todo efeito tem implementação Vulkan.
2. `CpuBackend` continua como referência de corretude e para testes.
3. **OpenCL vira backend opcional**, implementado só quando:
   - existir um caso concreto (um dos itens do §5); e
   - o interop sem cópia com Vulkan estiver verificado no runtime alvo (testar `cl_khr_external_memory_dma_buf`/`_opaque_fd` em rusticl e NEO).
4. A interface `ComputeBackend` continua permitindo OpenCL (não fechar a porta), e o runtime OpenCL é **detectado em runtime** (`dlopen` do ICD loader), nunca obrigatório.
5. Se essa direção for aceita, atualizar o `CLAUDE.md` §4 e §9.3 e registrar no ADR de compute.

---

## Fontes

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
- Arquivos locais do Omarchy 4.0.4: `/usr/share/omarchy/install/hardware/`
