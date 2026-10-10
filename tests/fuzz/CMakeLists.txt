# libFuzzer targets for parsers of untrusted files (CLAUDE.md §18). Clang only; opt-in preset.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzing requires Clang (use the fuzz preset; the Makefile's FUZZ_CXX=clang++).")
endif()
pkg_check_modules(SIMDJSON REQUIRED simdjson)

set(FUZZ_FLAGS -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
set(FUZZ_LINK -fsanitize=fuzzer,address,undefined)

add_executable(fuzz_cube
    fuzz_cube.cpp
    ${CMAKE_SOURCE_DIR}/libs/compositor/src/grade.cpp
    ${CMAKE_SOURCE_DIR}/libs/base/src/error.cpp)
target_include_directories(fuzz_cube PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/base/include ${CMAKE_SOURCE_DIR}/libs/compositor/include)
target_compile_options(fuzz_cube PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_cube PRIVATE ${FUZZ_LINK})

file(GLOB PROJECT_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/project/src/*.cpp)
file(GLOB TIMELINE_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/timeline/src/*.cpp)
file(GLOB BASE_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/base/src/*.cpp)
add_executable(fuzz_project fuzz_project.cpp
    ${PROJECT_LIB_SOURCES} ${TIMELINE_LIB_SOURCES} ${BASE_LIB_SOURCES})
target_include_directories(fuzz_project PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/timeline/include ${CMAKE_SOURCE_DIR}/libs/timeline/src
    ${CMAKE_SOURCE_DIR}/libs/project/include ${CMAKE_SOURCE_DIR}/libs/base/include)
target_include_directories(fuzz_project SYSTEM PRIVATE ${SIMDJSON_INCLUDE_DIRS})
target_compile_options(fuzz_project PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_project PRIVATE ${FUZZ_LINK})
target_link_libraries(fuzz_project PRIVATE ${SIMDJSON_LIBRARIES})

add_executable(fuzz_subtitles
    fuzz_subtitles.cpp
    ${CMAKE_SOURCE_DIR}/libs/project/src/subtitles.cpp
    ${CMAKE_SOURCE_DIR}/libs/base/src/error.cpp)
target_include_directories(fuzz_subtitles PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/project/include ${CMAKE_SOURCE_DIR}/libs/base/include)
target_compile_options(fuzz_subtitles PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_subtitles PRIVATE ${FUZZ_LINK})

set_target_properties(fuzz_cube fuzz_project fuzz_subtitles PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
file(MAKE_DIRECTORY
    ${CMAKE_BINARY_DIR}/cube-corpus
    ${CMAKE_BINARY_DIR}/project-corpus
    ${CMAKE_BINARY_DIR}/subtitles-corpus)
