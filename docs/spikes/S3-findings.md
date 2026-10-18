## Findings

1. **QSV adds nothing VA-API does not already give.** Both drive the same fixed-function
   block; QSV was 7–12 % slower in throughput and somewhat lighter on CPU. Keep the
   ADR-0004 order (VA-API → Vulkan Video → software) and do not add a QSV path or the libvpl
   dependency for decode.
2. **Hardware decode is about CPU, not throughput, for 1080p60 on this machine.** Every path
   is several times above 60 fps here. H.264 software decode is even faster than VA-API on
   this synthetic content, but takes 5–6 cores; the hardware paths take a quarter to half of
   one. `testsrc2` is easy to decode, so software numbers are an upper bound: real camera or
   screen-recording content will cost more CPU. This supports the M4 hardware decode pilot as
   a CPU/power measure, and still requires its end-to-end gate.
3. **AV1 through Vulkan Video still falls back** with `ANV_DEBUG` (S1 finding 4, reproduced).
4. **ProRes, not FFv1, is the proxy candidate.** ProRes is intra-only, so a seek costs about
   one frame (86 ms vs 74 ms at the start, mostly process startup); FFv1 seeks 2.5× slower
   than opening and its Vulkan decode runs at 19 fps, below real time. FFv1 stays useful as a
   lossless intermediate, not as an editing proxy.
5. **ProRes proxies must be encoded on the CPU for now**, because the Vulkan encoder output
   is invalid at 1080p. Vulkan compute decode of ProRes costs 27 % of one thread at 240 fps
   (software: 617 fps at six threads), so the GPU remains the cheaper way to play them.
6. **Power was not measured.** RAPL (`/sys/class/powercap/intel-rapl:0/energy_uj`) is
   root-only on this system; the script reports joules when it is readable. Power claims for
   any path stay open.

## Limits

Throughput is decoder-only: no mapping into OmaMovie's device, no compositing, no
presentation, no concurrent streams. One synthetic source per codec, one machine, frequency
scaling and thermals uncontrolled. Encode paths for export (VA-API vs QSV vs software) were not
measured; they belong to M7.

[Return to S3-decode-paths.md](S3-decode-paths.md)
