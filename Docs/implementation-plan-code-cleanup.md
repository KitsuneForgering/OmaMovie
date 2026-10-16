### Code cleanup during M6–M7

- [ ] Refactor code touched by the UI, project and export work when it improves a concrete change: clarify responsibilities and ownership, remove duplication, and keep the dependency direction in `CLAUDE.md` §5.2. Use Clean Code and Clean Architecture as guides, not as a reason to add layers or reorganize unrelated modules.
- [x] Review public/private header boundaries and include dependencies, taking SerenityOS `AK` as an organizational reference where it fits OmaMovie. Record the conventions adopted; keep public APIs in `include/oma/<module>/` and implementation details in `src/`. Preserve behavior and confirm the affected build, tests and formatting checks after each focused change. Use an ADR for a structural change.
  *Conventions and checks (2026-10-14)*: public API in `include/oma/<module>/`, everything else in `src/`
  (no public header includes a `src/` file); the §5.2 placement of FFmpeg, Qt and Vulkan headers is
  checked by `scripts/check-includes.sh` in the CI lint job, since system headers compile from anywhere;
  header cycles are caught by clang-tidy's `misc-header-include-cycle` (one, `audio/effect.hpp` ↔
  `effect_chain.hpp`, was broken on 2026-10-14). Not adopted from SerenityOS `AK`: a single
  utility module; OmaMovie's `libs/base` already plays that role without Qt.

### Targeted RAII and lifetime work

The existing ownership baseline is strong: FFmpeg contexts use `unique_ptr` with custom
deleters, Vulkan device creation uses `vk::raii`, GPU resources and the scheduler release
their work in destructors, and Qt files own their handles. Focus on paired operations
whose cleanup is still spread across error branches. This is a robustness and
maintainability task; do not expect a measurable playback or export speedup.

- [ ] In `libs/project/src/document.cpp::save`, give the `mkstemp` descriptor and temporary
  path explicit scope ownership. Preserve the checked `close` result, atomic rename and
  directory `fsync` order; disarm temporary-file removal only after a successful rename.
  Reuse the existing write/rename failure tests and add a focused check only for a newly
  reachable cleanup failure path.
- [ ] In `apps/omamovie/src/video_scheduler.cpp` and `viewer_decoder.cpp`, scope successful
  GPU admission so every exit calls `leave()`, and scope the decoder interruption callback
  so it never retains a job-local reference after the job ends. Keep the gate's existing
  cancellation and shutdown order. Exercise cancellation, decode failure, project switch
  and shutdown with the existing stress tests; add a regression if a gap is exposed.
- [ ] In `apps/omamovie/src/export_bundle.cpp`, separate descriptor ownership from the
  stage cleanup/recovery decision, including the lock-failure branch. Preserve the rule
  that backup-bearing stages survive for recovery. Use the existing crash-checkpoint and
  previous-output tests as the acceptance oracle.
- [ ] Inspect remaining raw Vulkan handles in `libs/compositor/src/vulkan_compositor.cpp`
  and the PipeWire loop/stream in `libs/audio/src/pipewire_output.cpp` only when modifying
  those paths or when a lifetime failure is reproduced. Their enclosing destructors
  already cover partial creation. Prefer a local owner with a clear dependency order;
  avoid a generic wrapper layer for handles with different release rules.

Done when each changed path has one identifiable owner per resource, release order is
explicit, the affected failure/cancellation and sanitizer checks pass, and same-machine
performance baselines show no meaningful regression. Record any structural ownership
change in an ADR.

[← Implementation plan](implementation-plan.md)
