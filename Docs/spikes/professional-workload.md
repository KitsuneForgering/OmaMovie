# Professional workload spike (experimental)

This spike asks whether OmaMovie's **current** decoder and compositor can process a bounded
three-source workload near a professional frame deadline. It does not add a product feature or
change the v0.1 hardware support matrix.

## Why this workload

There is no single maximum professional edit. [RED's V-RAPTOR X specification](https://docs.red.com/955-0225/955-0225_V1.7%20Rev-C%20RED%20PS%2C%20V-RAPTOR%20%5BX%5D%208K%20VV%20Operation%20Guide%20HTML/Content/B_TechSpecs/Specs_V-RAPTOR%20%5BX%5D.htm)
lists 8K 17:9 recording up to 120 fps in selected REDCODE settings;
[DaVinci Resolve Studio](https://www.blackmagicdesign.com/products/davinciresolve/color)
advertises 32K/120 fps and multiple GPUs. These are capability ceilings, not a claim that one
editor must play several such streams at full quality without proxies.

The useful stress case here is **three independently decoded pictures composed into a 4K60
output**: a graded base, a translucent blurred picture in picture, and a screened, sharpened
picture in picture. With three P010 3840 × 2160 sources at 60 fps, decoded surfaces alone are
about 4.48 GB/s (3 bytes/pixel × three sources × 60); this excludes reference frames,
compositor intermediates, upload, display and encoder traffic. An optional 8K60 output with
one 8K and two 4K sources probes the larger render and memory envelope. The input codec can
be HEVC Main 10, which OmaMovie supports on its tested Intel path. This is a **10-bit SDR**
stress case; the current HDR path does not perform tone mapping. REDCODE RAW, debayering,
HDR grading, temporal effects, multicamera switching, audio, subtitles and export are outside
this diagnostic.

## Run

Build only the diagnostic:

```sh
cmake --preset spikes -DCMAKE_BUILD_TYPE=Release
cmake --build build/spikes --target professional_workload_spike -j2
```

Use the Release build for timings; a Debug build is only a functional smoke check.

Smoke check with the generated small fixture (the same file is opened by three independent
decoders; it is not a professional performance result):

```sh
build/spikes/tools/spikes/professional_workload_spike 320 180 30 12 software \
  tests/fixtures/generated/h264_30fps_aac.mp4 \
  tests/fixtures/generated/h264_30fps_aac.mp4 \
  tests/fixtures/generated/h264_30fps_aac.mp4
```

For a repeatable 4K60 input, create three short files in a disposable directory. A high-quality
camera recording may be substituted, but record its codec, profile, bit depth, frame rate, GOP,
color metadata and storage device. The following synthetic files are **not RAW** and their
content is not a color-fidelity oracle:

```sh
mkdir -p /tmp/omamovie-professional-spike
for n in 0 1 2; do
  ffmpeg -hide_banner -nostdin -y -f lavfi -i "testsrc2=size=3840x2160:rate=60:duration=4" \
    -vf "hue=h=$((n * 30)),format=yuv420p10le" -c:v libx265 -preset ultrafast \
    -x265-params "pools=2:keyint=120:colorprim=bt709:transfer=bt709:colormatrix=bt709:range=limited" \
    "/tmp/omamovie-professional-spike/source${n}.mp4"
done
```

`testsrc2` begins as an 8-bit pattern converted to a 10-bit encoded format. The fixture
exercises the Main 10 decode and surface path, not true 10-bit source precision.

Then run either mode; `hardware` requires all three decoders to stay on a hardware path and
fails instead of silently reporting a software run. The FPS argument is a **deadline** and
must be checked against the printed nominal source rates. Use a graphical host with the
actual target GPU; llvmpipe is useful only for a smoke check.

```sh
build/spikes/tools/spikes/professional_workload_spike 3840 2160 60 180 hardware \
  /tmp/omamovie-professional-spike/source{0,1,2}.mp4
```

The shell brace in that example expands to three paths. For the optional 8K case, supply
an actual 8K source followed by two 4K sources and set `8192 4320 60`; do not upscale the
4K file and call the resulting decode an 8K test. Avoid starting that case on hardware whose
memory budget cannot hold the input surfaces and compositor intermediates.

## Reading the report

The output records selected Vulkan device, actual decode path per source, codec, bit depth,
nominal input rate, p50/p95/p99/max decode and render wall time, sequential deadline overruns,
throughput, process CPU time per frame and peak process RSS. Run at least five repetitions,
with a warm and cold media cache recorded separately, and keep every failed run. Record
commit, GPU, driver, FFmpeg version, power state, load and input hashes.

`sequential-over-deadline` counts the frames whose **decode plus synchronous GPU render**
exceeds `1000/FPS` ms. It is neither a count of frames dropped by the real player nor a proof
of 4K60 playback: the editor can overlap work, while this diagnostic omits Qt presentation,
audio and scheduling. Peak RSS excludes some device memory. A separate full-application test
must measure input-to-visible frame, dropped frames, A/V sync, VRAM and an independently
decoded export before claiming professional workflow performance. Compare another editor only
on the same host, source files, scene, preview policy and output contract.

The 4K60 case is useful if it either completes with bounded memory and a clear stage profile,
or fails with a reproducible decode, render or resource limit. The 8K case is exploratory:
failure on the reference laptop is information, not a v0.1 regression.

The [two 4K24 runs](../Research/evidence/2026-10-14-professional-workload-spike.txt) used
three copies of one HEVC Main 10 source on Iris Xe. Sequential totals were 64.72 and
43.03 ms p50 against a 41.67 ms target, with 24/24 and 19/24 deadline overruns. Host load
was uncontrolled and Qt presentation was absent. The runs expose a bottleneck and show
timing variability; they do not establish real player frame drops or a 4K60 result.
