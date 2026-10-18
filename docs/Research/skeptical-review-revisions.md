## Material revisions

| Earlier premise | Disposition and consequence |
|---|---|
| FFmpeg 9 announced August 2026 | Not corroborated by F1; local package/binary label is recorded separately; pin actual API provenance |
| Vulkan Video mature on all Intel/AMD codecs | Replace with profile/device/driver queries and negotiated-frame checks |
| NVENC universally ~5× faster than Vulkan encode | Withdraw ranking; compare equal-quality configurations in S7 |
| CUDA always forbids wait-before-signal | Withdraw blanket claim; specify exact semaphore type/version and liveness proof |
| OpenCL impossible without frame copies | Generic counterexample K4; keep exclusion as scope/maintenance choice |
| S2 is zero-copy by construction and proves correct content | Narrow to inspected application path and sampled luma; chroma, all frames, driver internals pending |
| FFmpeg ownership forces another device/copy | Incorrect generalization; ownership choice is about control and lifetime |
| iMovie uses FCP engine, proving our thesis | Transfer documented; internals not established; UI/pipeline thesis remains a hypothesis |
| Movie Maker 2012 proves semantic zoom | Historical behavior unverified; storyboard projection is our testable proposal |
| Resolve Linux means NVIDIA only / no PipeWire | Codec PDF is narrower; backend absence in a source is not incompatibility evidence |
| Stability is the main competitor complaint; market is empty | No representative sample; withdraw ranking and market-gap fact |
| Omarchy theme swap is atomic | Installed script removes then moves; adapter needs retries/last-good state |
| RGBA16F means HDR or accurate display | Storage precision is not color management; implement explicit transforms and oracle tests |
| Every TimeMap derives duration from source displacement | Freeze/reverse/ramps need domain and interpolation semantics; complement ADR-0002 before M5 |
| Leak is harmless ~200 KB per session | Prior report says per decoder; no retained raw reproduction here; broad suppressions can mask growth |

[Return to skeptical-review.md](skeptical-review.md)
