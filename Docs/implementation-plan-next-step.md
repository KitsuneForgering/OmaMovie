## 8. Next step

M5 and Intel M4 are closed; M7 has working project save/load and four selectable
export profiles. The [current audit](analysis/omamovie-2026-10-12.md) sets the next
work by OmaMovie's own release path, not by competitor feature count:

1. **Enforce the existing CI (M10):** the PR workflow runs, but `master` has no
   protection/ruleset requiring its checks. Require test/lint/fuzz success before
   merge; extend eligible sanitizer coverage to a bounded Session/app path.
2. **Close completed-output correctness (M7):** inject export write/sync failures,
   force a slow export across a project switch, and externally check selected
   decoded frames/samples and the caption sidecar from the pinned snapshot.
3. **Finish one editing path (M6/M8):** exercise import → source marks → exact edit
   → playback → save/reopen → export on Omarchy. Verify visible roll/slip/slide,
   model/UI agreement and a clear retimed-audio policy; record corrections and
   measured latency before claiming the workflow ready.
4. **Prove release packaging and recovery (M7/M10):** launch the installed package
   in a disposable Omarchy environment and reopen/recover the same test project.
5. **Expand only for a named hand-off (M8/M9):** portable project, one interchange
   format or stems need a receiving task and fidelity gate. They are not v0.1 gates.

Continue Intel device-loss and presentation-latency checks. Test NVIDIA, AMD and
hybrid paths only on available hardware; keep their claims out of v0.1 until then.
The measured 5,000-clip UI path does not justify a timeline/index or pooling rewrite.
For test/property and long-form expansion, follow the
[ranked engineering tasks](analysis/omamovie-2026-10-12-priorities.md).

[← Implementation plan](implementation-plan.md)
