## 2. Milestone overview

M0 and the M5 model are closed. M4 is closed for validated Intel paths. M2/M3,
M6/M7 and the remaining release checks stay scoped to their tested capabilities.

`M6 core edit workflow → M7 project/export correctness → M10 scoped validation`
`                                      → M11 targeted ownership/UI work → v0.1 decision`

After the scoped v0.1 decision, [M12](implementation-plan-m12-gpu-export-architecture.md)
tracks GPU-native export and ownership work as a separate phase. Its desired architecture
does not retroactively reopen M4/M5/M7 or block v0.1. The [v1.0 onboarding gate](implementation-plan-v1-onboarding.md)
closes product discoverability before v1.0 feature freeze, after the relevant interop
scope is settled and before the M6 manual workflow is signed off.

M8 contributes only features shipped in that workflow; its conditional pilots are
separate experiments. M9 interoperability and advanced color are separate support
claims. A release does not wait for every proposed feature in M8/M9 or for a marketplace
or agent pilot.

Intel and software paths can be implemented and validated now. AMD, NVIDIA and hybrid
paths require their own machines and evidence before advertising support. A missing
machine changes the support matrix, not the status of the Intel/software release path.
An interrupted export, unvalidated color path or incomplete project round trip still
blocks the corresponding claim on available hardware.
The [milestone reconciliation](analysis/omamovie-2026-10-12-milestones.md) and
[2026-10-12 assessment](analysis/omamovie-2026-10-12.md) prioritize one
complete import → source marks → timeline → playback → save/reopen → export task.
M8/M9 breadth remains conditional; milestone numbers and completion order remain.

[← Implementation plan](implementation-plan.md)
