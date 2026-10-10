# libs/timeline — timeline model, edit commands with undo/redo, and per-instant evaluation.
# Depends on base only: no Qt, GPU or FFmpeg (CLAUDE.md §5.2, §10).
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
oma_library(timeline SOURCES ${SOURCES} DEPS base)
