# oma_library / oma_test: the CMake replacements for the module.mk macros (ADR-0018).

# --- warning sets and sanitizer test environment (ADR-0001) ---
set(OMA_LIB_WARNINGS
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
    -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2
    -Wimplicit-fallthrough)
# GCC 16 reports -Wnull-dereference inside libstdc++'s unordered_map::find once TSan instruments
# it (a false positive); the other builds keep the check.
if(CMAKE_BUILD_TYPE STREQUAL "tsan")
    list(REMOVE_ITEM OMA_LIB_WARNINGS -Wnull-dereference)
endif()
set(OMA_TEST_WARNINGS -Wall -Wextra -Wshadow)
# Qt application sources use the same diagnostics as the engine.
set(OMA_APP_WARNINGS ${OMA_LIB_WARNINGS})
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # -Weverything covers production sources. Keep Werror for actionable diagnostics; these
    # categories report C++98/pre-C++23 compatibility, ABI layout, optimization hints or
    # existing FFmpeg/Vulkan raw-buffer and lifetime patterns rather than new regressions.
    # Review this debt list separately when those boundaries change.
    set(OMA_CLANG_EVERYTHING
        -Weverything
        -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-c++98-compat-local-type-template-args
        -Wno-pre-c++14-compat -Wno-pre-c++17-compat
        -Wno-pre-c++20-compat -Wno-pre-c++20-compat-pedantic -Wno-pre-c++23-compat
        -Wno-padded -Wno-weak-vtables -Wno-ctad-maybe-unsupported
        -Wno-nrvo -Wno-float-equal -Wno-shadow-uncaptured-local
        -Wno-switch-default -Wno-switch-enum -Wno-covered-switch-default
        -Wno-comma -Wno-global-constructors -Wno-exit-time-destructors
        -Wno-unused-macros -Wno-cast-function-type-strict
        -Wno-unsafe-buffer-usage -Wno-unsafe-buffer-usage-in-container
        -Wno-unsafe-buffer-usage-in-libc-call
        -Wno-lifetime-safety-cross-tu-suggestions
        -Wno-lifetime-safety-intra-tu-suggestions
        -Wno-lifetime-safety-intra-tu-constructor-suggestions
        -Wno-lifetime-safety-invalidation
        -Wno-lifetime-safety-return-stack-addr-moved)
    list(APPEND OMA_LIB_WARNINGS ${OMA_CLANG_EVERYTHING})
    list(APPEND OMA_APP_WARNINGS ${OMA_CLANG_EVERYTHING})
    # Cest's assertion macros, including the app test target, need these additional exclusions.
    list(APPEND OMA_TEST_WARNINGS ${OMA_CLANG_EVERYTHING}
        -Wno-used-but-marked-unused -Wno-disabled-macro-expansion
        -Wno-reserved-identifier -Wno-reserved-macro-identifier
        -Wno-old-style-cast -Wno-missing-variable-declarations
        -Wno-missing-prototypes -Wno-double-promotion
        -Wno-missing-noreturn -Wno-unreachable-code-loop-increment
        -Wno-conversion -Wno-sign-conversion)
endif()
# GCC 16 at -O2 reports -Wstringop-overflow and -Warray-bounds inside libstdc++'s
# uninitialized_copy / destroy_at for small vectors in two tests (test_video_writer.cpp,
# test_waveform.cpp; false positives, present before 2026-10-14). Tests only; the libraries keep
# both checks in every build.
if(CMAKE_BUILD_TYPE STREQUAL "release" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND OMA_TEST_WARNINGS -Wno-stringop-overflow -Wno-array-bounds)
endif()
if(OMA_WERROR)
    list(APPEND OMA_LIB_WARNINGS -Werror)
    list(APPEND OMA_TEST_WARNINGS -Werror)
    list(APPEND OMA_APP_WARNINGS -Werror)
endif()

# Both sanitizer builds also run offline and on open drivers only: the symbolizer must not fetch
# debug info over the network mid-test (DEBUGINFOD_URLS; it stalled runs by up to 40 s), and the
# proprietary NVIDIA Vulkan driver, uninstrumented and outside the v0.1 scope, takes ~10 s to start
# under TSan and corrupted its thread registry (measured 2026-10-13; CI has lavapipe only).
set(OMA_SANITIZER_ENV "DEBUGINFOD_URLS=" "VK_LOADER_DRIVERS_DISABLE=*nvidia*")
if(CMAKE_BUILD_TYPE STREQUAL "asan")
    set(OMA_TEST_ENV ${OMA_SANITIZER_ENV}
        "ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1:fast_unwind_on_malloc=0"
        "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tests/support/lsan.supp:print_suppressions=0"
        "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        "VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1")
elseif(CMAKE_BUILD_TYPE STREQUAL "tsan")
    set(OMA_TEST_ENV ${OMA_SANITIZER_ENV}
        # die_after_fork=0: the disk-full export check forks a child that starts FFmpeg's threads;
        # the child only writes and exits, and is judged by its status and output, not by TSan.
        "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:die_after_fork=0:suppressions=${CMAKE_SOURCE_DIR}/tests/support/tsan.supp"
        "VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1")
endif()

# oma_library(<name> SOURCES <srcs...> DEPS <project libs...>
#             [EXTERNAL_PUBLIC <items...>] [EXTERNAL_PRIVATE <items...>])
function(oma_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS;EXTERNAL_PUBLIC;EXTERNAL_PRIVATE" ${ARGN})
    oma_enforce_deps(${name} ${ARG_DEPS})
    add_library(oma_${name} STATIC ${ARG_SOURCES})
    add_library(oma::${name} ALIAS oma_${name})
    set_target_properties(oma_${name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    target_include_directories(oma_${name} PUBLIC
        ${CMAKE_SOURCE_DIR}/libs/${name}/include)
    target_include_directories(oma_${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/${name}/src)
    target_compile_options(oma_${name} PRIVATE ${OMA_LIB_WARNINGS})
    foreach(dep ${ARG_DEPS})
        target_link_libraries(oma_${name} PUBLIC oma_${dep})
    endforeach()
    target_link_libraries(oma_${name} PUBLIC ${ARG_EXTERNAL_PUBLIC})
    target_link_libraries(oma_${name} PRIVATE ${ARG_EXTERNAL_PRIVATE})
endfunction()

include(OmaTestTargets)
