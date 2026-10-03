# Cest (vendored)

OmaMovie test framework (ADR-0001).

| Field | Value |
|---|---|
| Upstream | https://github.com/KitsuneSemCalda/Cest |
| Commit | `3457ffcbc45deed4095ffe1151ef7d67dc135fc9` (v1.1.2 + 2 commits) |
| File | `cest.h` (sha256 `1c0d9a282b56429b679e8a1e8f9488ad97c4c805cffa7f362d010a6222ecdaba`) |
| License | BSD-3-Clause (`LICENSE`) |

Do not edit `cest.h` here. To update: copy `cest.h` from a newer commit or tag, update this
table and run every test in the `debug`, `asan` and `tsan` builds.

Tests never include this file directly: they use `tests/support/oma_test.hpp`, which
configures Cest and adds overloads for OmaMovie types.
