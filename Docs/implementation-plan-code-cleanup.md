### Code cleanup during M6–M7

- [ ] Refactor code touched by the UI, project and export work when it improves a concrete change: clarify responsibilities and ownership, remove duplication, and keep the dependency direction in `CLAUDE.md` §5.2. Use Clean Code and Clean Architecture as guides, not as a reason to add layers or reorganize unrelated modules.
- [x] Review public/private header boundaries and include dependencies, taking SerenityOS `AK` as an organizational reference where it fits OmaMovie. Record the conventions adopted; keep public APIs in `include/oma/<module>/` and implementation details in `src/`. Preserve behavior and confirm the affected build, tests and formatting checks after each focused change. Use an ADR for a structural change.
  *Conventions and checks (2026-10-14)*: public API in `include/oma/<module>/`, everything else in `src/`
  (no public header includes a `src/` file); the §5.2 placement of FFmpeg, Qt and Vulkan headers is
  checked by `scripts/check-includes.sh` in the CI lint job, since system headers compile from anywhere;
  header cycles are caught by clang-tidy's `misc-header-include-cycle` (one, `audio/effect.hpp` ↔
  `effect_chain.hpp`, was broken on 2026-10-14). Not adopted from SerenityOS `AK`: a single
  utility module; OmaMovie's `libs/base` already plays that role without Qt.

[← Implementation plan](implementation-plan.md)
