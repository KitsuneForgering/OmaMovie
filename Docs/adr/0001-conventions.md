## Conventions and license

### Warnings, formatting and lint
- Libs: `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
  -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2
  -Wimplicit-fallthrough -Werror`.
- Tests: `-Wall -Wextra -Wshadow -Werror` (Cest macros use C-style casts).
- Current Clang builds also use `-Weverything -Werror` for project targets, with documented
  diagnostic exclusions for C++23 compatibility, existing external buffer boundaries and
  Cest macros ([ADR-0021](0021-clang-diagnostics-and-release-optimization.md)). Production
  Qt app sources now receive the library warning set; GCC keeps the explicit flags above.
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

[Return to 0001-build-language-tests.md](0001-build-language-tests.md)
