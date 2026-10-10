# libs/project — the native project format (ADR-0007): JSON parsed with simdjson, written by a
# small pretty printer, saved atomically. Depends on the timeline model only; no Qt or FFmpeg.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
pkg_check_modules(SIMDJSON REQUIRED simdjson)
oma_library(project SOURCES ${SOURCES} DEPS timeline base)
target_include_directories(oma_project SYSTEM PRIVATE ${SIMDJSON_INCLUDE_DIRS})
target_link_libraries(oma_project PRIVATE ${SIMDJSON_LIBRARIES})
