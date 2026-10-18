## Application and media inventory — code state at `635ffad`

| Subsystem | Classification | Evidence and limit |
|---|---|---|
| Media probe/normalization | implemented and strongly tested | `libs/media/src/probe.cpp`, `tests/media/test_probe.cpp`; real corpus still narrow |
| Software decode and VFR seeking | implemented and strongly tested | `video_decoder.cpp`, `test_video_decoder.cpp`; not every camera codec tested |
| VA-API/DMABUF/Vulkan import | implemented but incomplete | `video_decoder.cpp:350`, `test_gpu_decode.cpp`; validated Intel, no driver-internal copy proof |
| Playback scheduler/seeking | implemented and strongly tested for Intel | `libs/playback/src/video_scheduler.cpp`; seek latency evidence M4; races need broader stress |
| Audio decode/resampling | implemented and strongly tested | `libs/media/src/audio_decoder.cpp`, `test_audio_decoder.cpp`; selected formats only |
| Audio mix and automation | implemented but incomplete | `libs/playback/src/timeline_audio.cpp`; retimed clips silent, block-partition oracle missing |
| CPU compositor | implemented and strongly tested on SDR scenes | `libs/compositor/src/cpu_compositor.cpp`, `tests/compositor`; reference is partly shared math |
| Vulkan compositor | implemented and strongly tested on Intel SDR subset | `vulkan_compositor.cpp`, CPU/pixel tests; other GPU/odd-depth matrix incomplete |
| Color/HDR | implemented but incomplete; correctness risk for unsupported input | `libs/compositor/src/color.cpp`; SDR BT.709/601 vectors, PQ/HLG no tone mapping |
| Export and encoder selection | implemented but incomplete | `apps/omamovie/src/exporter.cpp`, `tests/app/test_export_*`; decoded selected content/1080p long-form open |
| Captions SRT/VTT and sidecar | implemented and strongly tested on parser path | `libs/project/src/subtitles.cpp`, `test_subtitles.cpp`; ripple/edit workflow incomplete |
| Source viewer and In/Out | implemented but incomplete | `session.cpp`, `test_source_range.cpp`, `test_source_playback.cpp`; UI task check open |
| Timeline UI and edit tools | implemented but weakly tested as UI | `qml/TimelineView.qml`, `main.cpp` smoke; offscreen tests mainly inspect Session |
| Effects and presets | implemented but incomplete | `libs/timeline/src/effects.cpp`, `qml/EffectsDrawer.qml`; keyed effect parameters/packs open |
| Recordings | implemented and strongly tested for warm local scan | `platform/omarchy/recordings.*`, `test_recordings.cpp`; cold/network scan unknown |
| Jobs/background work | implemented but weakly tested in cross-workflow races | `libs/base/src/jobs.cpp`, `tests/base/test_jobs.cpp`; Session generation guards |
| Cache/thumbnail/waveform | implemented and strongly tested for persistence/corruption | `libs/base/src/disk_cache.cpp`, `test_disk_cache.cpp`; hit rate not measured |
| ActionRegistry | implemented but incomplete as automation | `apps/omamovie/src/action_registry.cpp:47,228`; stable discrete IDs, no parameter schema |
| Session and QML integration | architecture risk; implemented | `session.hpp:236`, `session.cpp`; many responsibilities and context-chain QML IDs |
| GUI smoke/Qt Test | implemented but incomplete | `main.cpp:752`, `tests/app/main.cpp`; real-window smoke manual, QML tree not in PR CI |
| Benchmarks | implemented but incomplete | `docs/Research/evidence/`, audit flags; no full 10k/3h or same-task comparator |
| Interchange | planned | `implementation-plan-m9-interop-color.md`; no OTIO/FCPXML importer claim |
| Media bins/search/proxies | planned | M8/M9 pilots; library and relink exist, scalable organization not proved |

The export path decodes with FFmpeg, composites on CPU/Vulkan, reads back rendered
frames and encodes through FFmpeg/VA-API. `DMA-BUF` import avoids an application
readback on the Intel preview path; internal driver copies are not measured. Software
fallback uploads decoded pixels, and export reads back each rendered frame. Retain
FFmpeg for codecs/containers; OmaMovie's owned value is policy, timestamps, mapping,
frame lifetime, cache and user-visible error handling. A custom codec adds a large
critical surface without a demonstrated workflow benefit.

Agent-assisted development risks visible in this sample: M5 retained a stale “not
yet” list after later work shipped; QML has repeated `pictureColorItem` names and
the smoke once selected an invisible one; preview tests can reuse the production
command and still miss a wrong UI target. Require a regression at the affected
boundary, review deleted/old routes when adding one, and inspect model plus UI
effects for a gesture. No broad dead-code or duplicate-core finding is established
from this audit; do not add another controller or automation engine speculatively.

[← Assessment](omamovie-2026-10-12.md)
