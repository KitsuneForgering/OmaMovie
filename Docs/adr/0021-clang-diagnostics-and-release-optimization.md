# ADR-0021 — Clang diagnostics and measured Release optimization

**Status:** Accepted 2026-10-18. Amends the warning policy in ADR-0001 and the CMake
configuration in ADR-0018.

## Context

The engine libraries already had a broad GCC/Clang warning set with `-Werror`, but the Qt
app used the smaller test set. Release fixed `-O2 -g -DNDEBUG`, with no simple way to run
an `-O3` or link-time optimization trial. The project needs consistent diagnostics without
making unsupported C++98 compatibility or a compiler's struct-padding suggestions into
release blockers. Optimization claims require same-fixture measurements.

## Decision

- Compile project libraries, the app, tests and fuzz targets with Clang `-Weverything`
  and, by default, `-Werror`. GCC retains its explicit warning set because it has no
  `-Weverything` flag.
- Exclude Clang categories that are incompatible with the project's C++23 choice, describe
  layout/optimization style rather than defects, or flag widespread existing FFmpeg/Vulkan
  raw-buffer and lifetime code that needs separate ownership work. Test targets have extra
  exclusions for the bundled Cest assertion macros. Keep the exact list in
  `cmake/OmaTargets.cmake`; a newly emitted, non-exempt diagnostic fails the build.
- Apply the library warning set to production Qt app sources. Fix the current explicit
  conversion diagnostics rather than relaxing that set.
- Keep Release at `-O2` by default. Offer `OMA_RELEASE_OPT_LEVEL=3` and
  `OMA_ENABLE_RELEASE_IPO=ON` for controlled trials; CMake checks IPO toolchain support.
  Neither option implies a speedup. Keep `-march=native` out of distributed artifacts.

## Evidence and consequences

With Clang 23, a syntax-only inventory over 163 library/app translation units found
hundreds of diagnostics from compatibility, padding, weak-vtable, raw-buffer and
optimization-hint categories. This is the reason for the documented exclusions, not proof
that those boundaries are safe. The default GCC Debug and Release app builds pass with
the expanded production warning set. A full Clang Debug build of app, libraries and tests
passes with `-Weverything -Werror`; `fuzz_cube` and `fuzz_probe` link, and the other fuzz
drivers pass syntax checks. The `-O3`/IPO configuration compiled and linked `oma-project`
with LTO. Workload correctness and performance comparisons are still required before
changing the Release default. CI continues to compile GCC and Clang configurations.
