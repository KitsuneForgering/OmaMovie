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
if(OMA_WERROR)
    list(APPEND OMA_LIB_WARNINGS -Werror)
    list(APPEND OMA_TEST_WARNINGS -Werror)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "asan")
    set(OMA_TEST_ENV
        "ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1:fast_unwind_on_malloc=0"
        "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tests/support/lsan.supp:print_suppressions=0"
        "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        "VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1")
elseif(CMAKE_BUILD_TYPE STREQUAL "tsan")
    set(OMA_TEST_ENV
        "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:suppressions=${CMAKE_SOURCE_DIR}/tests/support/tsan.supp"
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

# oma_test(<name> SOURCES <srcs...> LIBS <project libs...>
#          [EXTERNAL <items...>] [WORKING_DIRECTORY <dir>])
function(oma_test name)
    cmake_parse_arguments(ARG "" "WORKING_DIRECTORY" "SOURCES;LIBS;EXTERNAL" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/support ${CMAKE_SOURCE_DIR}/third_party/cest)
    target_compile_options(${name} PRIVATE ${OMA_TEST_WARNINGS})
    foreach(lib ${ARG_LIBS})
        target_link_libraries(${name} PRIVATE oma_${lib})
    endforeach()
    target_link_libraries(${name} PRIVATE ${ARG_EXTERNAL})
    oma_add_test(${name} ${ARG_WORKING_DIRECTORY})
endfunction()

# Register an executable (or an arbitrary command) with CTest. run_test.sh appends the Cest name
# filter from `make test FILTER=...`; the sanitizer environment is applied per test.
function(oma_add_test name)
    add_test(NAME ${name}
        COMMAND sh ${CMAKE_SOURCE_DIR}/tests/support/run_test.sh $<TARGET_FILE:${name}>)
    set(wd ${CMAKE_SOURCE_DIR})
    if(ARGN)
        set(wd ${CMAKE_SOURCE_DIR}/${ARGN})
    endif()
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${wd})
    if(OMA_TEST_ENV)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "${OMA_TEST_ENV}")
    endif()
endfunction()
