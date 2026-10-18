## 4. Decisions and ADRs

| Decision | When | ADR | Initial proposal |
|---|---|---|---|
| Build, C++23, test framework, license | M0 | 0001 | **Accepted**: GNU Make, C++23, Cest |
| Time representation + speed curves | M0 | 0002 | **Accepted**: `RationalTime` + piecewise monotonic `TimeMap` |
| Threading and job system | M0 | 0003 | **Accepted**: pipeline threads + `JobPool` |
| GPU frame, synchronization, decode policy | M2 | 0004 | **Accepted**: wrapped `AVVkFrame`, VA-API → Vulkan Video → software |
| Qt Quick ↔ compositor | M1 | 0005 | Single device through `fromDeviceObjects` |
| Color space + libplacebo | M1 | 0006 | **Accepted**: linear BT.709 RGBA16F, sRGB display; own shaders, libplacebo deferred |
| Project format | M7 | 0007 | **Accepted**: versioned pretty JSON, simdjson, `Timeline::restore` |
| ProjectIR and preservation | M9 | 0008 | `CLAUDE.md` §16 |
| Cache | M7 | 0009 ✅ | Key derived from inputs, `$XDG_CACHE_HOME/omamovie` |
| Canvas coordinates | M8 | 0010 | Decide before the aspect ratio becomes changeable |
| Transitions | ahead of M8 | 0011 | **Accepted**: clip-owned, centered on the cut, media handles |
| Clip time maps (freeze, reverse, ramps) | M5 | 0013 | **Accepted**: constant or segments (linear, freeze, linear-speed ramp), exact |
| Connected clips | M5 | 0014 | **Accepted**: anchor to a primary's source position, re-attached after every edit |
| Modular effects and project instances | before expansion in M7/M8 | 0016 | **Accepted**: effect definitions and project instances separated; package identity/version and keyframes deferred |
| Executable third-party effect host | conditional pilot | later ADR | Isolated process and compatibility/resource contract; contingent on the data-only pack pilot |

ADR-0004 already records application-owned device/frame/decode choices; do not restart their
approval as if nothing had been implemented. S4/ADR-0005 cover the shared-device
import diagnostic; the Intel interactive GPU handoff and SDR color path are implemented and tested.
Presentation latency, device loss and non-Intel hardware remain open gates.
The review corrects the rationale without silently changing accepted decisions. Any resulting
architecture change gets a new ADR under the existing convention.

[← Implementation plan](implementation-plan.md)
