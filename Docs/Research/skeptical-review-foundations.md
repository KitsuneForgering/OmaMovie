## Foundations and premise review

| Decision-critical claim | Foundation and mechanism | Premise status and boundary | Discriminating check |
|---|---|---|---|
| Shared queue is safe | Vulkan host synchronization vs GPU dependencies, queue retrieval valid usage (K1/K2); Qt tagged code (Q3) | Contract verified; compatibility inference identifies Q1; runtime behavior pending | S4 with actual Qt build, validation layers, concurrent submission and teardown |
| Imported frame needs no host copy | External-memory allocation/format/layout/lifetime; FFmpeg frame contract (F2) | API/source contract documented; S2 historical report; driver-internal copies unmeasured | Source/trace transfer accounting, multiple-frame YUV comparison and reuse stress |
| Hardware decode is supported | Profile/extent/chroma/depth queries (K3) plus negotiated frame format | Capability listing is insufficient; one Intel observation cannot generalize | Same media per API/device, reject silent fallback |
| Float output gives correct color | Distinct input/working/output transforms (D1), explicit metadata | Current SDR code observed; complete HDR/display handling absent | Independent vectors/ramps, alpha/range/chroma, preview/export agreement |
| No OpenCL interop exists | Khronos external-memory/semaphore sample (K4) | Universal claim contradicted; installed runtime matrix unknown | Concrete kernel/runtime/import experiment only if scope is reopened |
| Rational timestamps avoid accumulated rounding | Exact arithmetic and explicit rescaling; sample/frame units | Preference justified for arbitrary timebases; does not imply all curve samples are exact | Boundary/overflow and TimeMap domain/interpolation tests |
| File save survives a crash | Linux durability contract (S1); file/directory sync are distinct | File sync alone does not prove rename durability | Failure injection and recovery on supported filesystems |
| Simple UI improves editing | Interaction semantics, task completion/errors/recovery, baseline comparison | Product hypothesis, no local users observed | M6 pilot; preserve precise track/time control |
| Linux codec friction exists | Versioned B2 matrix | Supported for named Resolve 20 rows; no generic NVIDIA/PipeWire exclusion | Current edition + same-file task comparison before external claims |

These references apply because they specify the affected interfaces/units/tasks. They do
not substitute vendor prestige for evidence. Multiple summaries of one announcement are
not independent corroboration. Documentary contradictions trigger revision of the exact
premise, rather than averaging incompatible versions or inventing controversy.

[Return to skeptical-review.md](skeptical-review.md)
