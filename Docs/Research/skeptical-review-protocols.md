## Validation protocols

All protocols below are **proposed, not executed in this review**. Set scene/settings and
acceptance values before collecting results; record changes to exploratory criteria.

| Gate | Inputs, comparator and measurements | Acceptance / rejection |
|---|---|---|
| S2 follow-up | Synthetic CFR/VFR, 8/10-bit, chroma/color patterns; SW reference plus independent vectors; many frames, seeks, failed submits, shutdown | No validation-layer errors/corruption/stale reuse; bounded lifetime; count explicit copies and state driver-copy visibility limits |
| S3/S5/S7/S8 performance | Fixed representative scene, identical decode/output and encode-quality settings; warm-up + at least 5 repetitions, end-to-end timestamps, p50/p95/p99/max, drops, CPU/GPU/RAM/memory, power if instrument available | For claimed 1080p60 scene, proposed p99 ≤ 16.67 ms and no sustained drops over 10 minutes; reject hidden fallback/output mismatch/unbounded growth; means alone do not pass |
| S4/Q1 | Actual Qt/Wayland session and build, queue flags/retrieval, 2 videos + image, resize/minimize, decode+render concurrency, cancellation/teardown | Valid queue retrieval and dependency protocol; no validation errors/deadlock/readback in preview; explicit display transform; unsupported flag path must fail or select verified alternative |
| S6/color | Current shaders vs installed libplacebo and independently specified SDR vectors; full/limited ranges, primaries/transfer, alpha, chroma siting, missing metadata, PQ/HLG | Specify supported SDR errors/tolerances before measurement; correct fallback/notice for unsupported HDR; CPU/GPU agreement alone is insufficient |
| M4 audio | Synthetic audiovisual sync impulses plus real recordings, 10-minute runs, 44.1/48 kHz, device changes, seeks, underruns, no-audio case | Proposed sync pilot bound ≤ one output-video-frame interval after startup/seek settles, with no growing drift; define presentation/latency measurement; TSan does not validate RT deadlines |
| M6 UI | Same task sequence in drawer and side-inspector/ordinary-track baselines, wide/half/narrow, different fonts, mouse/keyboard | All required actions reachable, recoverable undo, no surprise linked-track corruption; record errors/time and revise on regressions; no population preference from a tiny pilot |
| M7 persistence | Versioned fixtures, forced failures at write/sync/rename/directory-sync, disk full, old/new versions | Recover old or complete new file, no corrupted acknowledged save; errors surfaced; durability statement names tested filesystem |
| M9 interchange | Pinned OTIO library/schema, actual app exports with known edits, fractional/unsupported times and effects | Preserve declared subset and explicit conversion error/report; no silent data loss or unproved round-trip claim |

[Return to skeptical-review.md](skeptical-review.md)
