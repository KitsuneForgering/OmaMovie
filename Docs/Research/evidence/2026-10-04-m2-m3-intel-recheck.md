# M2/M3 Intel recheck — 2026-10-04

The checks below ran outside the filesystem sandbox so Vulkan could see the development
machine's Intel Iris Xe (TGL GT2), Mesa 26.2.2, `/dev/dri/renderD128`. They extend the
existing S1–S6 evidence; they do not test AMD, NVIDIA or a hybrid laptop.

## Runtime capability report

Command: `make -j4 test FILTER=prerequisites` after adding `DeviceInfo`'s advertised
interop and video fields. Exit code 0; the filtered GPU suite reported 5 checks passed.
The selected device reported:

| Prerequisite | Runtime report |
|---|---|
| DMA-BUF import | yes |
| DRM format modifiers | yes |
| External semaphore fd | yes |
| Vulkan Video decode queue | no |
| Advertised Vulkan Video codec operations | `0x0` |

The render node and extension combination are consistent with S1's default-driver
observation and the current VA-API → DMA-BUF → Vulkan path. These flags are prerequisites,
not codec/profile support. S1's actual FFmpeg negotiated-format tests and S5's image/time
checks provide separate execution evidence. A hybrid machine is still needed to verify
selection, power behavior and identity when more than one physical GPU is present.

## Independent decoded-frame color check

Command: `make -j4 test FILTER=libswscale` on the same Intel device. Exit code 0; the
filtered compositor suite reported 5 checks passed. For four interior pixels of the
generated YUV patch frame, maximum difference between Vulkan linear output and
`libswscale`'s nonlinear RGB output after BT.1886 decoding was `0.02220` (test bound
`0.025`). `ffprobe` reports the Y4M fixture's color fields as unknown, so this checks
the untagged SD/BT.601 fallback. It does not validate tagged BT.709/2020 frames, chroma
siting, HDR tone mapping, display presentation or export.

The previous sandbox run used llvmpipe and produced the same sampled difference. A passing
test on this Intel driver is useful for the stated scene only; it is not a general color
accuracy claim.

## Pipeline cache on Intel

With a temporary `XDG_CACHE_HOME`, `make -j4 test FILTER=encodes` passed on the Iris Xe
and wrote an 85,175-byte pipeline cache file. Repeating the same command with that cache
present also passed. This checks persistence and reuse without a rendering failure; no
startup-time comparison was measured. The earlier llvmpipe check covered an empty-header
cache and a corrupted-file fallback.

## Full test suite

`make -j4 test` passed on the same Intel device after the M2 capability-report changes.
