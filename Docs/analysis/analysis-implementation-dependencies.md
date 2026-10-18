## 3. Grafo de Dependências (CMake)

Enforced via [`oma_enforce_deps`](cmake/OmaDependencyGraph.cmake#L13–34):

```
base  <-  gpu  <-  media  <-  compositor
base  <-  audio
base  <-  timeline
base  <-  playback? (playback depende base,gpu,media,audio,timeline) — ver linha 28: ALLOWED_DEPS_playback = base gpu media audio timeline ✓
base  <-  project-ir (previsto)
project-ir <- importers (previsto)
timeline, project-ir <- project (previsto: project depende timeline e project-ir; atual tem DEPS timeline base)
```

Implementado hoje ([linhas 13–28](cmake/OmaDependencyGraph.cmake#L13–L28)):
- `ALLOWED_DEPS_base = ""`
- `ALLOWED_DEPS_gpu = base`
- `ALLOWED_DEPS_media = base gpu`
- `ALLOWED_DEPS_compositor = base gpu media`
- `ALLOWED_DEPS_audio = base`
- `ALLOWED_DEPS_timeline = base`
- `ALLOWED_DEPS_playback = base gpu media audio timeline`
- `ALLOWED_DEPS_project-ir = base` (previsto)
- `ALLOWED_DEPS_importers = base project-ir` (previsto)
- `ALLOWED_DEPS_project = base timeline project-ir` (previsto) — mas [CMakeLists project](libs/project/CMakeLists.txt#L8): `oma_library(project SOURCES ${SOURCES} DEPS timeline base)` (não menciona project-ir). Isso indica diferença entre target (CLAUDE.md) e atual (implementado). **Lacuna de consistência** entre grafo previsto e CMake atual.

Verificação: libs sem Qt (Vulkan headers públicos apenas onde necessário). Ex.: [gpu CMake](libs/gpu/CMakeLists.txt#L6): `VULKAN_INCLUDE_DIRS` PUBLIC (headers públicos usam tipos Vk* — `resources.hpp` inclui `vulkan_core.h`). FFmpeg confinado a `src/` de media ([media CMake](libs/media/CMakeLists.txt#L9–L12): include dirs PRIVATE). Timeline não inclui Vulkan/Qt/FFmpeg — confirmado ([timeline model.hpp](libs/timeline/include/oma/timeline/model.hpp) só inclui base e STL).

[Return to analysis-implementation.md](analysis-implementation.md)
