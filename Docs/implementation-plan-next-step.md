## 8. Next step

M5 and Intel M4 are closed; M7 has working project save/load and four selectable export profiles.
The FilmCraft comparison changes the order of the remaining user-visible work:

1. **Correct completed output first (M7):** audio decode errors stop export, captions
   use its start snapshot, and injected errors at all four rename steps restore prior files.
   Restart recovery now detects interrupted stages and distinguishes a completed pair;
   `SIGKILL` tests cover each publication step. An immediate project switch keeps the
   original 30-frame output and caption. Next inject write failures, force a slow
   export across a project switch, then verify selected decoded content and long outputs.
2. **Complete one editing path (M6/M8):** source playback and marked-range placement now
   have headless checks; verify the visible roll/slip/slide path and declare an audio policy
   for retimed clips. Measure the full
   import → edit → save → reopen → export workflow against the current UI on Omarchy.
3. **Expand output only against a hand-off task (M7/M9):** destination choices and a portable
   project first; then one professional codec or interchange format with independently
   checked round-trip fidelity. Stems and richer titles stay conditional pilots.

Continue Intel device-loss and presentation-latency checks. Test NVIDIA, AMD and hybrid
paths only when their hardware is available; keep those claims out of v0.1 meanwhile. Per-frame viewer
allocations remain a profiling question: earlier 1080p composition was 3.5 ms p50, so
there is no evidence yet that a pooling rewrite should displace the workflow work.

[← Implementation plan](implementation-plan.md)
