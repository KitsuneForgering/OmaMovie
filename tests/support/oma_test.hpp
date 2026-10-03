#pragma once

// Entry point to the Cest test framework for OmaMovie tests (ADR-0001).
//
// Rules for test files (Cest macros take code blocks as macro arguments):
//   1. Include this header LAST, after project and standard headers. Cest defines short
//      function-like macros (test, it, expect, describe...) that clash with names used
//      inside some standard headers (for example std::bitset::test).
//   2. No top-level commas inside describe/it blocks: the preprocessor splits macro
//      arguments on commas that are not inside parentheses. Template arguments with
//      commas, lambda captures like [&a, &b] and brace initializers {1, 2} break the
//      block. Use auto, [&], helper functions, or wrap the expression in parentheses.
//   3. Project APIs must not use the names describe, test, it, expect, bench, beforeEach,
//      afterEach, beforeAll or afterAll: test files would expand them as Cest macros.
//   4. Call expect() only from the test thread; Cest is not built thread-safe here.

// Cest redefines feature-test macros that glibc already set; drop ours to avoid a
// redefinition warning (these macros only matter before the first libc include).
#undef _POSIX_C_SOURCE
#undef _DEFAULT_SOURCE

// cest.h silences -Wstrict-prototypes, a C-only option, which GCC reports in C++
// (-Wpragmas). Suppress that one diagnostic around the include.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#include "cest.h"
#pragma GCC diagnostic pop

// Cest has no overloads for unsigned integers (calls would be ambiguous).
static inline cest_value_t cest_value(unsigned int v) {
    return cest_int(static_cast<long long>(v));
}
static inline cest_value_t cest_value(unsigned long v) {
    return cest_int(static_cast<long long>(v));
}
static inline cest_value_t cest_value(unsigned long long v) {
    return cest_int(static_cast<long long>(v));
}
