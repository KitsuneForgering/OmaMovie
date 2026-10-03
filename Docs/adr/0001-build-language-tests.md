# ADR-0001 — GNU Make build, C++23, tests with Cest, code conventions

- **Status:** Accepted (2026-10-02)
- **Milestone:** M0

## Context

The project needs a reproducible build, sanitizers, tests and linting from the first
commit (`Docs/implementation-plan.md`, M0). The original plan proposed CMake; the
maintainer prefers not to use CMake. The test framework was open (GoogleTest or Catch2);
the maintainer chose Cest, a header-only framework they wrote.

## Decision

### Build: non-recursive GNU Make
- A single `Makefile` at the root. Each lib declares its sources in `libs/<name>/module.mk`
  with `oma_library`; each test suite in `tests/<name>/module.mk` with `oma_test`.
- Modes through `BUILD=debug|release|asan|tsan`, each in `build/<mode>/`.
- **The dependency graph from CLAUDE.md §5.2 is enforced by Make**
  (`ALLOWED_DEPS_<lib>`): a forbidden dependency or an unregistered lib is an error before
  anything compiles.
- Automatic header dependencies (`-MMD -MP`); objects depend on the Makefile.
- `make compdb` writes `compile_commands.json` for clangd and clang-tidy.
- Project libs are static (`liboma_<name>.a`).

### Language
- **C++23** (`-std=c++23`, no GNU extensions), for `std::expected`, `std::move_only_function`,
  `std::format`, `std::source_location`. GCC 16 and Clang 22 (Arch) support it.
- `__int128` is used in `libs/base` for exact time arithmetic, through `__extension__`.

### Tests: Cest
- `third_party/cest/cest.h`, vendored and pinned to a commit (see `third_party/cest/README.md`).
- Tests include `tests/support/oma_test.hpp`, never `cest.h` directly. That header configures
  Cest, works around a pragma warning from `cest.h` in C++ and adds overloads for unsigned
  integers.
- One binary per lib (`build/<mode>/tests/<lib>_tests`), one suite per file, and a
  `main.cpp` that calls the `run_*_tests()` functions.
- `make test FILTER=<pattern>` uses the Cest filter; `JUNIT_DIR=<dir>` writes JUnit (CI).

Rules imposed by Cest (macros that take code blocks as arguments):
1. `oma_test.hpp` is the **last** include of a test file.
2. **No top-level commas** inside `describe`/`it`: template arguments with commas, captures
   such as `[&a, &b]` and `{1, 2}` break the block. Use `auto`, `[&]`, helper functions or
   parentheses.
3. **Project APIs do not use the names** `describe`, `test`, `it`, `expect`, `bench`,
   `beforeEach`, `afterEach`, `beforeAll`, `afterAll` (they become macros in tests). That is
   why `oma::Error` has `summary()`, not `describe()`.
4. `expect()` only on the test thread.

### Warnings, formatting and lint
- Libs: `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
  -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2
  -Wimplicit-fallthrough -Werror`.
- Tests: `-Wall -Wextra -Wshadow -Werror` (Cest macros use C-style casts).
- `.clang-format` (LLVM base, 4 spaces, 100 columns); `make format` / `make format-check`.
- `.clang-tidy` with `WarningsAsErrors: '*'`; `make tidy` runs on the libs.

### Naming conventions
| Element | Convention | Example |
|---|---|---|
| Types, enums, aliases | `PascalCase` | `RationalTime`, `JobState` |
| Enum values | `PascalCase` | `Rounding::Nearest` |
| Functions, methods, variables | `snake_case` | `time_to_frame` |
| Private members | `snake_case_` | `timebase_` |
| Constants (`constexpr`, globals) | `kPascalCase` | `kCategoryCount` |
| Namespaces | `snake_case` | `oma::frame_rates` |
| Headers | `#pragma once`, `include/oma/<lib>/<name>.hpp` | |

All repository content is in English: identifiers, code comments, log messages, docs,
ADRs, READMEs, CI step names and commit messages (CLAUDE.md §22).

### License
The repository is MIT. Arch's FFmpeg is built with `--enable-gpl` (and Qt is LGPL/GPL):
distributed binaries that link that FFmpeg are subject to the GPL. MIT is GPL-compatible,
so the code stays MIT; binary distribution follows the GPL terms. Revisit when packaging
(M7) is defined.

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
