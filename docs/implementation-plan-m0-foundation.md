### M0 — Foundation ✅ (2026-10-02)

**Deliverables**
- [x] Build/development dependencies declared by the PKGBUILD; `make deps` reads it.
- [x] Non-recursive `Makefile` (GNU Make, ADR-0001): `BUILD=debug|release|asan|tsan`, one `module.mk` per lib and per test suite.
      **Superseded (2026-10-10):** the build is now CMake with GNU Make as a thin front-end
      (ADR-0018). The `BUILD=` names and the documented `make` commands are unchanged. This
      historical record of the M0 build keeps its original wording.
- [x] Full warning set with `-Werror` on the libs; `.clang-format`, `.clang-tidy` (warnings are errors), `.editorconfig`, `.gitignore`.
- [x] **Dependency graph from §5.2 enforced by Make**: a forbidden dependency or an unregistered lib fails before compiling. The graph now includes `base`, `gpu`, `media`, `compositor`, `audio` and `timeline`.
- [x] `libs/base`:
  - [x] `Rational`, `RationalTime`, `TimeRange`, `FrameRate`, `SampleRate`; 128-bit `rescale` with overflow detection and explicit rounding; exact comparison across timebases.
  - [x] `oma::Error` + `Result<T>` (`std::expected`).
  - [x] Logging facade with the §19 categories, no Qt.
  - [x] Job system: thread pool, cooperative cancellation, progress, ordered shutdown.
- [x] Tests with **Cest** (vendored in `third_party/cest`, pinned to a commit), `make test`, filter and JUnit.
- [x] CI (GitHub Actions, Arch container): g++ and clang++ × debug/asan/tsan/release, plus format and clang-tidy; Conventional Commits check; tag-based release; Dependabot.
- [x] Fixture generator (`tests/fixtures/generate.sh`): H.264 CFR/NTSC/VFR, 10-bit HEVC, AV1+Opus, rotation, truncated file, 44.1 kHz WAV, PNG.
- [x] ADR-0001 (Make, C++23, Cest, conventions, license), ADR-0002 (time and `TimeMap` for speed ramps), ADR-0003 (threading and job system).

**Done when**: `make test` passes locally and in CI; time tests cover 23.976/29.97/59.94,
44.1/48/96 kHz, hours in a 90 kHz timebase, rounding and overflow; `CLAUDE.md` §3 reflects
the real commands.

**Result**: 45 tests (109 assertions) pass in `debug`, `release`, `asan` and `tsan` with
GCC 16 and Clang 22, locally and in CI; `make tidy` and `make format-check` are clean. TSan
found a lock-order inversion in the logging test, fixed and documented in the sink contract.
The first CI run caught a const-correctness warning from the container's newer clang-tidy.

**Research**: `CLAUDE.md` §6, [capcut](Research/capcut.md).

[← Implementation plan](implementation-plan.md)
