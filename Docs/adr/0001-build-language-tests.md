# ADR-0001 — GNU Make build, C++23, tests with Cest, code conventions

- **Status:** Accepted (2026-10-02) — **build portion superseded by [ADR-0018](0018-cmake-build.md)
  (2026-10-10).** The GNU Make decision below is historical; the language, Cest, conventions and
  license decisions remain in force.
- **Milestone:** M0

## Evidence review (2026-10-03)

This records maintainer/build choices, not a measured superiority of Make/Cest. Package
and compiler versions are scoped to the recorded environment. Distribution depends on the
actual linked FFmpeg/Qt configuration; [FFmpeg's licensing guidance](https://ffmpeg.org/legal.html)
distinguishes GPL-enabled and nonfree builds. Keep the source MIT preference; perform the
concrete binary/source/license packaging check in M7 rather than treating this as legal clearance.

## Context

The project needs a reproducible build, sanitizers, tests and linting from the first
commit (`Docs/implementation-plan.md`, M0). The original plan proposed CMake; the
maintainer prefers not to use CMake. The test framework was open (GoogleTest or Catch2);
the maintainer chose Cest, a header-only framework they wrote.

## Decision

- [Build, language and tests](0001-build-and-tests.md)
- [Conventions and license](0001-conventions.md)

## Alternatives considered
- **CMake + Ninja**: the Qt 6 and ecosystem default; rejected by maintainer preference.
- **Meson**: good Qt and pkg-config support; not proposed.
- **GoogleTest / Catch2**: more features (parameterized fixtures, rich matchers); Cest was
  chosen because it is the maintainer's, header-only and enough for M0.

## Consequences
- Qt (M4/M6) without CMake requires invoking `moc`, `rcc` and `qmlcachegen`/`qmltyperegistrar`
  from the Makefile, using `pkg-config` for Qt6. `qt_add_qml_module` is CMake-only; QML type
  registration has to be done by hand or with custom rules. Evaluate in M4.
- Cest limitations (commas, reserved names) must be followed by every test; violations show
  up as confusing compile errors inside the macros.
- Cest issues to report upstream: `#pragma GCC diagnostic ignored "-Wstrict-prototypes"`
  triggers `-Wpragmas` in C++; `_DEFAULT_SOURCE` is redefined.
