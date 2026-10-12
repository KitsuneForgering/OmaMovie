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

include(cmake/OmaBuildOptions.cmake)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

include(OmaDependencyGraph)
include(OmaTargets)

enable_testing()
include(cmake/OmaProjectTargets.cmake)
include(cmake/OmaDeveloperTargets.cmake)
