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
    ${CMAKE_SOURCE_DIR}/libs/base/src/error.cpp
    ${CMAKE_SOURCE_DIR}/libs/base/src/error_strings.cpp)
target_include_directories(fuzz_cube PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/base/include ${CMAKE_SOURCE_DIR}/libs/compositor/include)
target_compile_options(fuzz_cube PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_cube PRIVATE ${FUZZ_LINK})

file(GLOB PROJECT_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/project/src/*.cpp)
file(GLOB TIMELINE_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/timeline/src/*.cpp)
file(GLOB BASE_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/base/src/*.cpp)
file(GLOB PROJECT_IR_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/project-ir/src/*.cpp)
add_executable(fuzz_project fuzz_project.cpp
    ${PROJECT_LIB_SOURCES} ${TIMELINE_LIB_SOURCES} ${PROJECT_IR_LIB_SOURCES} ${BASE_LIB_SOURCES})
target_include_directories(fuzz_project PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/timeline/include ${CMAKE_SOURCE_DIR}/libs/timeline/src
    ${CMAKE_SOURCE_DIR}/libs/project/include ${CMAKE_SOURCE_DIR}/libs/project-ir/include
    ${CMAKE_SOURCE_DIR}/libs/base/include)
target_include_directories(fuzz_project SYSTEM PRIVATE ${SIMDJSON_INCLUDE_DIRS})
target_compile_options(fuzz_project PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_project PRIVATE ${FUZZ_LINK})
target_link_libraries(fuzz_project PRIVATE ${SIMDJSON_LIBRARIES})

add_executable(fuzz_subtitles
    fuzz_subtitles.cpp
    ${CMAKE_SOURCE_DIR}/libs/project/src/subtitles.cpp
    ${CMAKE_SOURCE_DIR}/libs/base/src/error.cpp
    ${CMAKE_SOURCE_DIR}/libs/base/src/error_strings.cpp)
target_include_directories(fuzz_subtitles PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/project/include ${CMAKE_SOURCE_DIR}/libs/base/include)
target_compile_options(fuzz_subtitles PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_subtitles PRIVATE ${FUZZ_LINK})

file(GLOB IMPORTERS_LIB_SOURCES ${CMAKE_SOURCE_DIR}/libs/importers/src/*.cpp)
add_executable(fuzz_otio fuzz_otio.cpp
    ${IMPORTERS_LIB_SOURCES} ${PROJECT_IR_LIB_SOURCES} ${BASE_LIB_SOURCES})
target_include_directories(fuzz_otio PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/importers/include ${CMAKE_SOURCE_DIR}/libs/importers/src
    ${CMAKE_SOURCE_DIR}/libs/project-ir/include ${CMAKE_SOURCE_DIR}/libs/base/include)
target_include_directories(fuzz_otio SYSTEM PRIVATE ${SIMDJSON_INCLUDE_DIRS})
target_compile_options(fuzz_otio PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_otio PRIVATE ${FUZZ_LINK})
target_link_libraries(fuzz_otio PRIVATE ${SIMDJSON_LIBRARIES})

add_executable(fuzz_edl fuzz_edl.cpp
    ${IMPORTERS_LIB_SOURCES} ${PROJECT_IR_LIB_SOURCES} ${BASE_LIB_SOURCES})
target_include_directories(fuzz_edl PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/importers/include ${CMAKE_SOURCE_DIR}/libs/importers/src
    ${CMAKE_SOURCE_DIR}/libs/project-ir/include ${CMAKE_SOURCE_DIR}/libs/base/include)
target_include_directories(fuzz_edl SYSTEM PRIVATE ${SIMDJSON_INCLUDE_DIRS})
target_compile_options(fuzz_edl PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_edl PRIVATE ${FUZZ_LINK})
target_link_libraries(fuzz_edl PRIVATE ${SIMDJSON_LIBRARIES})

# The probe goes through the media library (FFmpeg is the uninstrumented system library), so this
# target checks OmaMovie's handling of what FFmpeg reports rather than FFmpeg's own parsers. The
# media and base libraries are instrumented so coverage and ASan reach OmaMovie's probe code.
set(FUZZ_LIB_FLAGS -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all)
target_compile_options(oma_media PRIVATE ${FUZZ_LIB_FLAGS})
target_compile_options(oma_base PRIVATE ${FUZZ_LIB_FLAGS})
add_executable(fuzz_probe fuzz_probe.cpp)
target_compile_options(fuzz_probe PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_probe PRIVATE ${FUZZ_LINK})
target_link_libraries(fuzz_probe PRIVATE oma_media)

set_target_properties(fuzz_cube fuzz_project fuzz_subtitles fuzz_otio fuzz_edl fuzz_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
file(MAKE_DIRECTORY
    ${CMAKE_BINARY_DIR}/cube-corpus
    ${CMAKE_BINARY_DIR}/project-corpus
    ${CMAKE_BINARY_DIR}/subtitles-corpus
    ${CMAKE_BINARY_DIR}/otio-corpus
    ${CMAKE_BINARY_DIR}/edl-corpus
    ${CMAKE_BINARY_DIR}/probe-corpus)
