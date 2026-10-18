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

[Return to 0018-cmake-build.md](0018-cmake-build.md)
