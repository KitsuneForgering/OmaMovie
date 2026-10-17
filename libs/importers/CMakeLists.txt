# libs/importers — external projects to ProjectIR (ADR-0008, CLAUDE.md §16). Each format reads
# untrusted text with bounded parsers and produces only ProjectIR plus an import report.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
pkg_check_modules(SIMDJSON REQUIRED simdjson)
oma_library(importers SOURCES ${SOURCES} DEPS project-ir base)
target_include_directories(oma_importers SYSTEM PRIVATE ${SIMDJSON_INCLUDE_DIRS})
target_link_libraries(oma_importers PRIVATE ${SIMDJSON_LIBRARIES})
