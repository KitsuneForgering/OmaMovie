## 8. Next step

**First executable action for M12 (post-v0.1 architecture): finish the transfer baseline.**
The [first instrumented run](Research/evidence/2026-10-15-export-transfer-baseline.md)
already counts software compositor inputs, NV12 readbacks, writer row copies and VA-API
uploads on 1080p60 and 4K Intel exports. Next split `read_nv12()` into GPU conversion,
queue wait and host-copy timings; measure actual compositor staging, process CPU/frame,
memory/GPU use and downgrade reasons; repeat the same fixtures under controlled load with
independent frame/audio fidelity checks. Only then compare a GPU-native encoder-input
pilot. This remains a baseline task, not a zero-copy claim. See [M12](implementation-plan-m12-gpu-export-architecture.md).

**v1.0 closure order:** settle the bounded [interop scope](implementation-plan-m9-interop-color.md)
→ [first-opening tour and contextual tooltip coverage](implementation-plan-v1-onboarding.md)
→ [M6 manual workflow from a clean profile](m6-walkthrough.md) → fixes/polish found there
→ packaging and recovery → required CI gates → final regression/stress pass → feature
freeze → release candidate. The tour is a product gate before feature freeze, not new
editor feature breadth. Advanced VFX categories need a separately demonstrated workflow
need before becoming v1.0 blockers.

M5 and Intel M4 are closed; M7 has working project save/load and four selectable
export profiles. The [current audit](analysis/omamovie-2026-10-12.md) sets the next
work by OmaMovie's own release path, not by competitor feature count. Revised 2026-10-14
by the [evidence status](implementation-plan-evidence-2026-10-14.md): two measured
regressions in interaction come before new breadth.

0. **Paused hardware scrub (M11) — done 2026-10-14:** paused hardware frames decode on their
   own job worker with latest-request-wins; UI gap p95 221 → 12 ms at 1080p, 1323 → 12 ms at 4K
   ([async milestone](implementation-plan-async-interactive.md), ADR-0020). Left: the 4K
   composite still runs in Qt's sync phase (~11 ms).
0. **5,000-clip edits (M6):** fixed 2026-10-14 (QML bindings reading the whole clip list for its
   length; p95 now 33–93 ms). Keep the audit's raw samples; an intermittent ~90 ms split sample remains unexplained.

1. **Enforce the existing CI (M10):** a repository ruleset now protects `master`
   (2026-10-15): PR, linear history, resolved review threads, no force push/deletion,
   and required `ci` plus `conventional commits` checks. The `ci` aggregator on this
   branch gates test/GUI/lint/includes/fuzz jobs; confirm it runs on the first PR
   before closing this gate. Extend eligible sanitizer coverage to a bounded
   Session/app path. The versioned policy is in `.github/rulesets/master.json`.
2. **Completed-output correctness (M7) — done 2026-10-12:** write failures are injected
   with `RLIMIT_FSIZE`, a 1,200-frame export survives a project switch, and selected
   frames and samples are matched to their source by an independent decode. Two-hour 1080p
   exports with cuts, looks and a dissolve pass (2026-10-13; 2026-10-15 with 24 reordered cuts
   checked frame by frame, `tools/bench/long-export/run.sh`).
3. **Finish one editing path (M6/M8):** exercise import → source marks → exact edit
   → playback → save/reopen → export on Omarchy. Roll/slip/slide now have keyboard,
   pointer, menu and two-up paths with tests; the corrections, latency and retimed-audio
   policy need the [manual sessions](m6-walkthrough.md) before the workflow is called ready.
4. **Prove release packaging and recovery (M7/M10):** launch the installed package
   in a disposable Omarchy environment and reopen/recover the same test project.
5. **Expand only for a named hand-off (M8/M9):** portable project, one interchange
   format or stems need a receiving task and fidelity gate. They are not v0.1 gates.

Continue Intel device-loss and presentation-latency checks. Test NVIDIA, AMD and
hybrid paths only on available hardware; keep their claims out of v0.1 until then.
The 5,000-clip design (incremental model, level of detail) is not in question, but its
2026-10-06 numbers no longer hold; a timeline index or pooling rewrite still needs a profile.
For test/property and long-form expansion, follow the
[ranked engineering tasks](analysis/omamovie-2026-10-12-priorities.md).

[← Implementation plan](implementation-plan.md)
