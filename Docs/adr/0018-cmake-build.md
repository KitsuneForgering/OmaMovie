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

- **CMake (>= 3.25) is the build system.** Ninja is the generator. `CMakePresets.json` exposes the
  configure and build presets **debug, release, asan, tsan** (the existing `BUILD=` names). Project
  libraries are static (`liboma_<name>.a`); each lib and test suite has a `CMakeLists.txt` in place
  of its `module.mk`.
- **GNU Make stays as the user-facing entry point**, reimplemented as thin wrappers over
  `cmake`/`ctest`: `make`, `make test`, `make BUILD=asan test`, `make run-gui`, `make tidy`,
  `make format-check`, `make fixtures`, `make fuzz`, `make spikes`, `make oma-project`,
  `make install`, `make deps`, `make help`. The documented commands in CLAUDE.md §3 keep working.
  `FILTER=`, `JUNIT_DIR=`, `DESTDIR=`, `PREFIX=`, `WERROR=` and `V=` are preserved.
- **The §5.2 dependency graph is enforced by CMake.** An `ALLOWED_DEPS_<lib>` table and a policy
  function fail configuration on a forbidden or unregistered dependency, exactly as the Makefile
  did. This invariant is not optional: losing it is an architecture bug.
- **Qt.** `find_package(Qt6)` for the app; `AUTOMOC` replaces the manual `Q_OBJECT` scan and the
  `/usr/lib/qt6/moc` rule. **QML remains plain files** (not `qt_add_qml_module`): a development
  build reads the source tree and an installed build reads `share/omamovie/qml`, so the existing
  `qml_dir()` fallback, the smoke run and the install layout are unchanged.
- **Tests.** `enable_testing()` + `add_test` per suite; `make test` runs `ctest`. JUnit comes from
  `ctest --output-junit`. `tests/project/test_cli.sh` is registered as a test. Sanitizer
  environment (ASAN/UBSAN/LSAN/TSAN options and suppressions) is set per configuration.
- **Tools.** `tools/oma-project`, the `tools/spikes` prototypes and the `tests/fuzz` libFuzzer
  targets are CMake targets, kept opt-in exactly as today (spikes and fuzz stay Clang/pkg-config
  based).
- **Packaging.** `install()` reproduces the current tree (`bin`, `share/omamovie/qml`, desktop
  entry, icon, MIME package, license). The PKGBUILD keeps calling `make`; `cmake` and `ninja` move
  to `makedepends`. `WERROR=0` maps to the `OMA_WERROR=OFF` CMake option.

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
- CI (`.github/workflows/ci.yml`, `release.yml`), the PKGBUILD, `Docs/implementation-plan.md` and
  CLAUDE.md §3/§4/§5.1/§5.2 are updated. `cmake` and `ninja` become `makedepends`.
- The `--config`/preset names stay `debug`/`release`/`asan`/`tsan`, so muscle memory and CI matrices
  do not change. `build/<preset>/omamovie` remains the executable path (the app's relative QML
  lookup is preserved).
- ADR-0001 is marked superseded **in its build portion only**; C++23, Cest and the conventions it
  records still stand.
