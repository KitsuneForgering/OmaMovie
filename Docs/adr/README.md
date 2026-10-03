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
| 0004 | GPU frame abstraction and synchronization | Pending (M1) |
| 0005 | Qt Quick ↔ Vulkan compositor integration | Pending (M1) |
| 0006 | Working color space and libplacebo | Pending (M1) |
| 0007 | Native project format | Pending (M7) |
| 0008 | ProjectIR and preservation of external data | Pending (M9) |
| 0009 | Cache strategy | Pending (M7) |
| 0010 | Canvas coordinates | Pending (M8) |

Template: copy the structure of an existing ADR (Context, Decision, Alternatives, Consequences).
