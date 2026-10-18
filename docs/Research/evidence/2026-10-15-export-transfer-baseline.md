# Export transfer boundary baseline — 2026-10-15

This is a **single-run diagnostic**, not an A/B performance result or a zero-copy claim.
The executable was built from working tree `a29f81e` plus the instrumentation in this
change. Intel Core i5-1135G7 / Iris Xe, Mesa 26.2.2-arch1.1, FFmpeg 9.0.1. The export
uses its own Vulkan device and VA-API H.264 writer. The diagnostic was run in the user's
graphical session; the sandbox's Qt platform plugin aborted before an export began.

## Reproduce

```sh
cmake --build build/release --target omamovie -j8
build/release/omamovie --export-audit tests/fixtures/generated/screenrecording_like.mp4 /tmp/omamovie-export-transfers-1080p60.mp4
build/release/omamovie --export-audit "$HOME/.cache/omamovie-bench/fixtures/a2160.mp4" /tmp/omamovie-export-transfers-4k24.mp4
ffprobe -v error -show_entries stream=index,codec_name,width,height,nb_frames,duration -show_entries format=duration -of json /tmp/omamovie-export-transfers-1080p60.mp4
ffprobe -v error -show_entries stream=index,codec_name,width,height,nb_frames,duration -show_entries format=duration -of json /tmp/omamovie-export-transfers-4k24.mp4
ffmpeg -v error -i /tmp/omamovie-export-transfers-1080p60.mp4 -f null -
ffmpeg -v error -i /tmp/omamovie-export-transfers-4k24.mp4 -f null -
```

Generate the cached 4K fixture using the recipe in
`tools/bench/filmcraft-equivalent/fixtures.sh` if absent: `a2160.mp4` is 3840×2160,
24000/1001 fps, ten seconds of noisy `testsrc2`, H.264. The 1080p60 fixture is generated
by `make fixtures` (one second, 60 video frames). No benchmark media is committed here.

## Observed explicit boundaries

| Boundary | 1080p60, 60 frames | 4K23.976, 240 frames | Source of counter |
|---|---:|---:|---|
| Software frame inputs to Vulkan compositor | 60; 186,624,000 source-plane bytes | 240; 2,985,984,000 bytes | `exporter.cpp`, `VideoFrame::plane()` sizes before `VulkanCompositor::render` |
| Vulkan → CPU NV12 readbacks | 60; 186,624,000 bytes; 189.62 ms | 240; 2,985,984,000 bytes; 1536.16 ms | `VulkanCompositor::read_nv12` result and elapsed call time |
| CPU NV12 row copies into writer AVFrame | 186,624,000 bytes; 46.00 ms | 2,985,984,000 bytes; 651.90 ms | `VideoWriter::write_nv12` memcpy loop |
| CPU AVFrame → VA-API surface uploads | 60; 186,624,000 bytes; 186.12 ms | 240; 2,985,984,000 bytes; 2668.89 ms | successful `av_hwframe_transfer_data` calls |

1080p60: 745 ms total, 80.5 exported fps; per frame build/decode 0.49 ms,
render+readback 10.97 ms, writer worker 5.47 ms. 4K: 10,193 ms total, 23.5 fps;
per frame build/decode 3.11 ms, render+readback 38.16 ms, writer worker 16.66 ms.
These are wall times with overlapping render/encode stages and one run each, so the
subtotals are **not additive** and do not show CPU time per frame. `read_nv12` includes
GPU color conversion, synchronization and host copy; its elapsed time does not isolate
those components. Source-plane bytes indicate data presented for compositor staging,
not a measurement of bus bandwidth. Driver-internal copies are outside these counters.
The startup logs can show a separate VA-API viewer decode; the export loop itself still
constructs `FrameSource(nullptr)` and logs software decode.

Independent FFmpeg decode completed without errors for both outputs. `ffprobe` found
60 H.264 frames at 1920×1080 plus AAC for 1080p60, and 240 H.264 frames at 3840×2160
plus AAC for 4K. This checks decodability, count and stream presence, not frame fidelity
or A/V sample alignment; those remain the M12 workload gates (existing app tests cover
selected exported frame/audio behavior on other fixtures).

## Next measurement

Split NV12 conversion, GPU wait and mapped-buffer host copy in the compositor diagnostic;
measure upload/staging wait separately, process CPU/frame, peak RAM/VRAM and GPU activity.
Repeat both fixtures under controlled load and record percentiles before using these
numbers as an optimization comparison. The Intel DMA-BUF encoder-input pilot follows
only after that baseline and a concrete ownership/synchronization contract.
