## Question, foundations and hypotheses

Question: which changes let OmaMovie edit and deliver longer material without losing work or
making common actions unresponsive? Baseline: current JSON/`simdjson` project, in-memory
timeline, Qt Quick shell, software-decoded viewer and no export. Failure costs include lost
edits and a final video whose image or sound differs from the preview.

The relevant foundation is a *quality scenario*: specify the workload and stimulus, then
measure the response rather than call an implementation “scalable” in isolation. The
[SEI scenario method](https://www.sei.cmu.edu/documents/704/2003_005_001_14213.pdf)
(Appendix A, pp. 43–44; 2003 technical report, accessed 2026-10-04) describes stimulus,
environment, response and response measure. Here the independent dimensions are source
duration, clip/track count, resolution/codec/GOP, effects, simultaneous streams, and output
duration. A parser benchmark measures only one dimension.

| Hypothesis | Mechanism and current evidence | What would change the decision |
|---|---|---|
| H1: simdjson is sufficient for the native project | `libs/project/src/document.cpp` reads a bounded 64 MiB JSON file into memory and parses it with simdjson DOM; the JSON contains references and edits, not video frames. ADR-0007 reports exact round trips but no large-project timing. | Real projects near the cap or measured load/save memory/latency dominate the workflow after simpler representation fixes. |
| H2: clip count will hurt the UI before duration alone | `Session::clips()` and `audioTracks()` build fresh `QVariantList`s of every clip; `Main.qml` uses several `Repeater`s over them. An edit copies the timeline snapshot and emits whole-sequence change signals. This is a source-level risk, not a benchmark. | Large-clip fixtures show stable UI time and memory, or profiling identifies a different dominant cost. |
| H3: random access and effects need distinct remedies | FFmpeg seeks to a keyframe and decoding reaches the requested frame. Kdenlive documents proxies for source decoding and preview renders for effect-heavy sections. OmaMovie's viewer currently uses software decode; its `FrameSource` has bounded decoder/history caches. | Matched footage shows proxy generation cost exceeds seek benefit, or effects are cheap enough without a preview cache. |
| H4: completion/recovery outweigh parser speed for long work | Atomic manual save exists, but autosave, relink UI and export remain open in M7. A long export has more opportunities for interruption and A/V drift. | Complete long-form workflow and failure-injection evidence show these paths already reliable. |

[Return to long-form-editing.md](long-form-editing.md)
