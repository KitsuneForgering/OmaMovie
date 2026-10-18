# S1 — Hardware inventory (Intel Iris Xe, TigerLake)

- **Date:** 2026-10-03
- **Milestone:** M1 (`docs/implementation-plan.md`)
- **Feeds:** ADR-0004 (GPU frame, synchronization, decode policy)
- **Reproduce:** `tools/spikes/s1-inventory.sh [output-dir]`

## Evidence review (2026-10-03)

The results below are **previously reported local experiments**, not rerun by the skeptical
review. Preserve them as historical observations of the named hardware/build/inputs.
The scripts/source exist, but the document does not retain the raw outputs, source-build
provenance and repetitions needed for independent reproduction. Package labels were
re-observed in [the environment snapshot](../Research/evidence/2026-10-03-local.txt);
they do not resolve the upstream-release discrepancy recorded in the audit.

Capability listings and actual successful decodes are separate evidence. VP9 is advertised
in the text but absent from the executed-decode table; do not count it as tested playback.
Flags/profile lists, resolutions and codec claims are limited to this setup. Extension
presence is necessary but insufficient for mapping. Compute-codec success does not select
a default proxy format without seek/quality/storage measurements. AMD defaults and NVIDIA
behavior remain unmeasured here.

## Question

Which codecs and profiles does the development machine decode and encode, through which
API (VA-API, QSV, Vulkan Video, Vulkan compute codecs), and with which caveats?

## Environment

| Component | Version |
|---|---|
| GPU | Intel Iris Xe Graphics (TGL GT2), device `0x9a49`, integrated |
| Kernel | 7.2.5-3-omarchy |
| Vulkan driver | Mesa 26.2.2 (ANV), Vulkan 1.4.354 |
| VA-API driver | intel-media-driver (iHD) 26.2.4, libva 2.24 |
| QSV | libvpl 2.17.0, vpl-gpu-rt 26.2.4 |
| FFmpeg | n9.0.1 (Arch build: `--enable-vulkan --enable-libvpl --enable-vaapi --enable-libplacebo`) |

## Method

- `vulkaninfo` (extensions, queues, `--show-video-props`) and `vainfo` for advertised capabilities.
- FFmpeg decode of 1 s 1080p test media with `-hwaccel X -hwaccel_output_format X`, reading the
  **pixel format FFmpeg finally chose** (`Format ... chosen by get_format()`, debug log). A
  hardware format means the hardware path ran; a software format means FFmpeg fell back.
- FFmpeg encode of 1 s 1080p test media per encoder, checked with `ffprobe`.
- Vulkan Video probed with and without `ANV_DEBUG=video-decode,video-encode`.

## Results

[Measurements](S1-results.md)

## Findings

[Policy and open items](S1-findings.md)
