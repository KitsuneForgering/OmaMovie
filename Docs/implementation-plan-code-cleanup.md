### Code cleanup during M6–M7

- [ ] Refactor code touched by the UI, project and export work when it improves a concrete change: clarify responsibilities and ownership, remove duplication, and keep the dependency direction in `CLAUDE.md` §5.2. Use Clean Code and Clean Architecture as guides, not as a reason to add layers or reorganize unrelated modules.
- [ ] Review public/private header boundaries and include dependencies, taking SerenityOS `AK` as an organizational reference where it fits OmaMovie. Record the conventions adopted; keep public APIs in `include/oma/<module>/` and implementation details in `src/`. Preserve behavior and confirm the affected build, tests and formatting checks after each focused change. Use an ADR for a structural change.

[← Implementation plan](implementation-plan.md)
