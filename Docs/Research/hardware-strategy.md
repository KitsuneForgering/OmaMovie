# Estratégia de hardware: síntese

> Síntese de [`vulkan.md`](vulkan.md), [`cuda.md`](cuda.md), [`opencl.md`](opencl.md) (OpenCL removido)
> e [`omarchy-integration.md`](omarchy-integration.md). É uma proposta para
> discussão: vira regra só depois de entrar no `CLAUDE.md` ou num ADR.

Objetivo: **usar todo o hardware disponível** (decode, encode e compute em
Intel, AMD e NVIDIA) **sem tirar o frame da VRAM** e sem obrigar o usuário a instalar nada além do que o Omarchy já instala.

---

## 1. Arquitetura proposta

```
                         ┌──────────────── libs/gpu ────────────────┐
                         │  VkInstance / VkDevice únicos (Vulkan-Hpp) │
                         │  lock de fila · VMA · memória importada    │
                         └───────┬──────────────┬──────────────┬──────┘
                                 │              │              │
        ┌────────────────────────▼──┐   ┌───────▼────────┐  ┌──▼──────────────────┐
        │ libs/media (FFmpeg 9)      │   │ libs/compositor│  │ apps/omamovie (Qt)   │
        │ hwcontext_vulkan sobre o   │   │ render graph → │  │ QQuickGraphicsDevice │
        │ device do OmaMovie         │   │ Vulkan (+ libplacebo?)│ ::fromDeviceObjects│
        │                            │   │ ComputeBackend │  │ QSGVulkanTexture     │
        │ Decode:                    │   │  ├ Vulkan (padrão)│ ::fromNative        │
        │  Intel/AMD: Vulkan Video   │   │  ├ CPU (referência)                     │
        │             ou VA-API→map  │   │  └ CUDA (opcional, PTX)                 │
        │  NVIDIA: NVDEC→interop     │   │                                         │
        │          ou Vulkan Video   │   └────────────────┘  └─────────────────────┘
        │ Encode:                    │
        │  Intel/AMD: Vulkan/VA-API  │        AVVkFrame (timeline semaphore por imagem)
        │  NVIDIA: NVENC             │        = Frame do OmaMovie, sem cópia
        │ Intermediários: ProRes/FFv1│
        │  em compute shader         │
        └────────────────────────────┘
```

---

## 2. Papel de cada API

| API | Papel | Obrigatória? |
|---|---|---|
| **Vulkan (Vulkan-Hpp)** | Device único; composição; preview; compute padrão de efeitos; decode/encode de vídeo em AMD/Intel; codecs em compute (ProRes, FFv1) | **Sim** |
| **VA-API** | Decode/encode maduro em Intel/AMD; mapeado para Vulkan pelo FFmpeg | Sim (via FFmpeg), escolhida em runtime |
| **NVDEC/NVENC** | Decode/encode na NVIDIA; NVENC é muito mais rápido que Vulkan encode | Sim na NVIDIA (via FFmpeg, sem toolkit) |
| **CUDA (driver API)** | Interop NVDEC → Vulkan; kernels opcionais | Carregada em runtime, só na NVIDIA |
| **CUDA (kernels)** | Efeitos com ganho medido na NVIDIA | Opcional, build opcional |
| ~~OpenCL~~ | **Removido** (decisão de 2026-10-02, ver `opencl.md`) | Não |
| **libplacebo** | Candidata para cor, tone mapping, escala, LUTs | A decidir (ADR) |

---

## 3. Seleção em runtime

Na inicialização, `libs/gpu` + `libs/media` montam uma **tabela de capacidades**
(registrada em log, categoria `gpu`, e exposta numa tela de diagnóstico):

| Pergunta | Fonte |
|---|---|
| Que codecs/perfis/profundidade de bits o Vulkan Video decodifica e codifica? | `vkGetPhysicalDeviceVideoCapabilitiesKHR` |
| O que o VA-API oferece? | `vaQueryConfigProfiles` / FFmpeg |
| Há NVDEC/NVENC? | Presença de `libnvcuvid`/`libnvidia-encode`, FFmpeg |
| Há CUDA? | `dlopen` de `libcuda` |
| Qual GPU é a primária (híbridos)? | Escolha do usuário ou heurística, sem acordar a dGPU à toa |

Para cada stream, o caminho de decode é escolhido por uma **política ordenada e
testável** (ex.: Intel H.264 → VA-API se benchmark favorecer, senão Vulkan
Video → software). Fallback para software é sempre possível, e toda degradação vai para o log (`CLAUDE.md` §19).

---

## 4. O que muda em relação ao `CLAUDE.md` atual

Propostas (dependem da sua aprovação):

1. ~~OpenCL como backend genérico~~ **Aplicado em 2026-10-02**: OpenCL removido;
   Vulkan Compute é o backend genérico (`CLAUDE.md` §4 e §9.3).
2. **Vulkan-Hpp explícito** na stack: `vk::raii`, sem exceções
   (`VULKAN_HPP_NO_EXCEPTIONS` + `VULKAN_HPP_RAII_NO_EXCEPTIONS` com `std::expected`), include confinado a `libs/gpu`/`libs/compositor`. Afeta §4 e §22.
3. **Device Vulkan único compartilhado com FFmpeg e Qt** como invariante. Afeta §7 e §9.4.
4. **Política de CUDA**: NVDEC/NVENC via FFmpeg sem toolkit; kernels só via PTX
   (clang) + `dlopen`; build CUDA opcional. Afeta §4 e §9.3.
5. **Notebooks híbridos**: um device primário por pipeline. Afeta §7.
6. **Integração Omarchy**: app_id `omamovie`, regra de opacidade, tema via
   `omarchy-theme-color`, módulo de plataforma isolado. Afeta §11.
7. **Pacote `vulkan-headers`** (e `vulkan-tools` para diagnóstico) nas dependências de build.

---

## 5. Spikes de validação (antes do código definitivo)

Cada spike tem um critério mensurável. Eles substituem a parte técnica do "primeiro experimento" do `CLAUDE.md` §24.

| # | Spike | Critério de sucesso | Hardware |
|---|---|---|---|
| S1 | `vulkaninfo` + `vainfo`: inventário de capacidades na Iris Xe | Tabela de codecs × API documentada | Iris Xe |
| S2 | FFmpeg 9 usando um `VkDevice` criado pelo OmaMovie (Vulkan-Hpp) | Decode H.264/HEVC/AV1 em `AVVkFrame` sem readback | Iris Xe |
| S3 | Vulkan Video vs. VA-API→map: frame time, CPU, potência | Números por codec; política de seleção definida | Iris Xe |
| S4 | Qt Quick sobre o mesmo device exibindo a imagem do compositor | Preview a 60 fps sem cópia para CPU; lock de fila sem deadlock | Iris Xe |
| S5 | Composição de 2 vídeos + 1 imagem (transform, crop, opacity, YUV→RGB) | Frame time medido em 1080p60 | Iris Xe |
| S6 | libplacebo sobre imagens do OmaMovie | Conversão de cor/escala sem cópia; decisão para o ADR | Iris Xe |
| S7 | NVDEC → Vulkan (interop) vs. Vulkan Video na NVIDIA | Escolha do caminho NVIDIA por medição | **Precisa de máquina NVIDIA** |
| S8 | AMD (RADV) | Mesmas medições de S3 | **Precisa de máquina AMD** |

---

## 6. Riscos principais

| Risco | Mitigação |
|---|---|
| Sincronização de fila entre Qt, FFmpeg e compositor | S4 cedo; um único mecanismo de lock; ADR-0005 |
| Diferenças de driver em DMA-BUF/modificadores | Usar o mapeamento do FFmpeg; fallback com cópia GPU→GPU registrada em log |
| CUDA não aceita wait-before-signal | Agendador garante a ordem; testes específicos |
| Sem hardware AMD/NVIDIA para testar | Planejar máquinas de teste ou contribuidores; CI só cobre caminhos de software |
| Custo de compilação do Vulkan-Hpp | Include confinado + PCH; avaliar módulo `vulkan` |
| libplacebo não aceitar imagens externas como esperado | S6 antes de decidir |

---

## Próximos passos sugeridos

1. Você decide os pontos restantes do §4 (principalmente libplacebo).
2. Atualizar o `CLAUDE.md` com o que for aprovado.
3. Instalar `vulkan-headers vulkan-tools libva-utils` e rodar S1.
4. M0 (build system + `libs/base`) em paralelo com S2–S4.
