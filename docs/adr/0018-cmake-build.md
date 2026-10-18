# ADR-0018 — CMake as the build system, GNU Make as a thin front-end

**Status: Accepted (2026-10-10).** Supersedes the build portion of
[ADR-0001](0001-build-language-tests.md) (GNU Make). The language, test framework, code
conventions and license decisions of ADR-0001 remain in force.

## Context

ADR-0001 chose a non-recursive GNU Make build: one `Makefile` plus one `module.mk` per lib and
per test suite, `BUILD=debug|release|asan|tsan`, the §5.2 dependency graph enforced by Make
(`ALLOWED_DEPS_<lib>`), `moc` invoked by hand for headers declaring `Q_OBJECT`, versioned Qt
headers reached with `-isystem` hacks, FFmpeg/Qt/simdjson/PipeWire flags assembled with
`pkg-config`, and shaders compiled with `glslc` through a custom rule.

The maintainer now wants CMake as the build system of record, with GNU Make kept as a friendly
front-end that hides the long commands. CMake is the ecosystem default for Qt 6: `find_package(Qt6)`
resolves the correct include directories (no versioned-header workaround), `AUTOMOC` removes the
hand-rolled `Q_OBJECT` scan, `CMakePresets.json` expresses the four configurations declaratively,
CTest runs the suites and emits JUnit natively, and `CMAKE_EXPORT_COMPILE_COMMANDS` produces
`compile_commands.json` without a bespoke rule.

## Decision

[Read the decision](0018-decision.md)

## Alternatives considered

- **Keep GNU Make** (rejected by maintainer preference now): no dependency change, but keeps the
  hand-rolled moc/Qt/glslc plumbing and a bespoke test runner.
- **CMake and Make permanently side by side**: two build systems to keep green; rejected as double
  maintenance for no gain.
- **Meson**: good Qt and pkg-config support; not proposed.
- **`qt_add_qml_module`** (QML embedded in resources + automatic type registration): more idiomatic
  CMake, but it changes how QML is loaded at run time and would require reworking `qml_dir()`, the
  smoke run and the install layout; rejected for this migration.

## Consequences

- One build system of record (CMake); GNU Make is sugar. `module.mk` files are removed.
- The dependency-graph guard is reimplemented in CMake (`cmake/OmaDependencyGraph.cmake`); a test or
  a deliberate check must keep it honest.
- CI (`.github/workflows/ci.yml`, `release.yml`), the PKGBUILD, `docs/implementation-plan.md` and
  CLAUDE.md §3/§4/§5.1/§5.2 are updated. `cmake` and `ninja` become `makedepends`.
- The `--config`/preset names stay `debug`/`release`/`asan`/`tsan`, so muscle memory and CI matrices
  do not change. `build/<preset>/omamovie` remains the executable path (the app's relative QML
  lookup is preserved).
- ADR-0001 is marked superseded **in its build portion only**; C++23, Cest and the conventions it
  records still stand.
