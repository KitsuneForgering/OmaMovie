## Build, language and tests

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

[Return to 0001-build-language-tests.md](0001-build-language-tests.md)
