# OmaMovie — CMake build (ADR-0018). The GNU Makefile is a thin front-end over these presets.
#
#   cmake --preset debug && cmake --build --preset debug && ctest --preset debug
#   cmake --preset release | asan | tsan
#   cmake --preset spikes   (M1 prototypes)   | cmake --preset fuzz (libFuzzer, Clang)
#
# Each lib and test suite declares its own CMakeLists.txt; cmake/OmaDependencyGraph.cmake enforces
# the §5.2 graph at configure time; cmake/OmaTargets.cmake holds the oma_library/oma_test helpers.

cmake_minimum_required(VERSION 3.25)
project(omamovie LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE debug CACHE STRING "Build type (debug release asan tsan)" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json for clangd and clang-tidy; written into the build dir (the Make front-end
# links it at the root).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The four configurations of ADR-0001, now CMake build types. asan/tsan are not standard CMake
# types; their flags live in CMAKE_CXX_FLAGS_<TYPE>. Release keeps -g (packagers strip separately).
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fno-omit-frame-pointer -fsanitize=thread" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined -fno-sanitize-recover=all"
    CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN "-fsanitize=thread" CACHE STRING "" FORCE)

option(OMA_WERROR "Treat warnings as errors" ON)
option(OMA_BUILD_TESTS "Build the Cest test suites" ON)
option(OMA_BUILD_APP "Build the Qt Quick app" ON)
option(OMA_BUILD_SPIKES "Build the disposable M1 spikes" OFF)
option(OMA_BUILD_FUZZ "Build the libFuzzer targets (Clang only)" OFF)
set(OMA_LICENSE_NAME "omamovie" CACHE STRING "License directory name used by install()")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

include(OmaDependencyGraph)
include(OmaTargets)

enable_testing()

# --- libraries (CLAUDE.md §5.1) ---
add_subdirectory(libs/base)
add_subdirectory(libs/gpu)
add_subdirectory(libs/media)
add_subdirectory(libs/compositor)
add_subdirectory(libs/audio)
add_subdirectory(libs/timeline)
add_subdirectory(libs/playback)
add_subdirectory(libs/project)

add_subdirectory(tools/oma-project)

# --- tests ---
# Fixtures: the ffmpeg generator, stamped so it runs once (like the old TEST_PREREQS).
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/tests/fixtures/generated/.stamp
    COMMAND ${CMAKE_SOURCE_DIR}/tests/fixtures/generate.sh
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_SOURCE_DIR}/tests/fixtures/generated/.stamp
    DEPENDS ${CMAKE_SOURCE_DIR}/tests/fixtures/generate.sh
    COMMENT "Generating test fixtures (ffmpeg)")
add_custom_target(fixtures DEPENDS ${CMAKE_SOURCE_DIR}/tests/fixtures/generated/.stamp)

if(OMA_BUILD_TESTS)
    add_subdirectory(tests/base)
    add_subdirectory(tests/gpu)
    add_subdirectory(tests/media)
    add_subdirectory(tests/compositor)
    add_subdirectory(tests/audio)
    add_subdirectory(tests/timeline)
    add_subdirectory(tests/playback)
    add_subdirectory(tests/project)
endif()

# The app and its headless Session tests: debug and release only (as in ADR-0001).
if(OMA_BUILD_APP AND (CMAKE_BUILD_TYPE STREQUAL "debug" OR CMAKE_BUILD_TYPE STREQUAL "release"))
    add_subdirectory(apps/omamovie)
endif()

if(OMA_BUILD_SPIKES)
    add_subdirectory(tools/spikes)
endif()
if(OMA_BUILD_FUZZ)
    add_subdirectory(tests/fuzz)
endif()

# --- developer targets (format, tidy, compdb) ---
file(GLOB_RECURSE OMA_FORMAT_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/libs/*.cpp ${CMAKE_SOURCE_DIR}/libs/*.hpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp ${CMAKE_SOURCE_DIR}/tests/*.hpp)
file(GLOB_RECURSE OMA_TIDY_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/libs/*.cpp)

add_custom_target(format
    COMMAND clang-format -i ${OMA_FORMAT_FILES}
    COMMENT "clang-format")

add_custom_target(format-check
    COMMAND clang-format --dry-run --Werror ${OMA_FORMAT_FILES}
    COMMENT "clang-format --dry-run")

# Generated SPIR-V includes must exist before clang-tidy parses vulkan_compositor.cpp.
if(TARGET oma_compositor_shaders)
    add_custom_target(tidy
        COMMAND clang-tidy -p ${CMAKE_BINARY_DIR} --quiet ${OMA_TIDY_SOURCES}
        COMMENT "clang-tidy"
        DEPENDS oma_compositor_shaders)
else()
    add_custom_target(tidy
        COMMAND clang-tidy -p ${CMAKE_BINARY_DIR} --quiet ${OMA_TIDY_SOURCES}
        COMMENT "clang-tidy")
endif()

add_custom_target(compdb
    COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "compile_commands.json -> build")
