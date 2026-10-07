# Architecture Decision Records

Decisions that significantly affect the architecture (CLAUDE.md §20). Each ADR records
context, decision, alternatives and consequences. An accepted ADR is not edited to change
its decision: a new decision supersedes the old one in a new ADR, which marks the previous
one as superseded.

| ADR | Title | Status |
|---|---|---|
| [0001](0001-build-language-tests.md) | Build (GNU Make), C++23, tests with Cest, conventions | Accepted |
| [0002](0002-time-representation.md) | Time representation | Accepted |
| [0003](0003-threading-and-job-system.md) | Threading model and job system | Accepted |
| [0004](0004-gpu-frames-sync-decode-policy.md) | GPU frame abstraction, synchronization and decode policy | Accepted |
| [0005](0005-qt-vulkan-interop.md) | Qt Quick ↔ Vulkan compositor integration | Accepted (S4 import diagnostic; on-screen handoff pending) |
| [0006](0006-color-space-and-libplacebo.md) | Working color space, SDR stages and libplacebo | Accepted |
| [0007](0007-project-format.md) | Native project format: versioned JSON, simdjson | Accepted |
| 0008 | ProjectIR and preservation of external data | Pending (M9) |
| [0009](0009-cache.md) | Cache strategy | Accepted |
| 0010 | Canvas coordinates | Pending (M8) |
| [0011](0011-transitions.md) | Transitions between clips | Accepted |
| [0012](0012-color-grading.md) | Color grading: CDL, curves and 3D LUTs | Accepted |
| [0013](0013-clip-time-maps.md) | Clip time maps: freeze, reverse and speed ramps | Accepted |
| [0014](0014-connected-clips.md) | Connected clips: anchors to a primary clip's content | Accepted |
| [0015](0015-titles.md) | Titles: generated clips rasterized by the app | Accepted |
| [0016](0016-video-effects.md) | Video effects: an ordered stack of built-in operations | Accepted |

Template: copy the structure of an existing ADR (Context, Decision, Alternatives, Consequences).

## Evidence review (2026-10-03)

Accepted ADRs retain their historical decisions and now carry dated evidence-limit notes.
The [skeptical review](../Research/skeptical-review.md) distinguishes documentary support
from tests performed. ADR-0005 resolves Q1 for the static S4 preview; M4 must
complete live playback admission. ADR-0006 must define SDR input/working/display
stages and independent validation. A complementary TimeMap domain/interpolation ADR is
required before M5 freeze/reverse/ramps. Implementation checkboxes cannot accept an ADR by
implication. A changed architectural decision supersedes the old ADR in a new record.
