# libs/project-ir — ProjectIR, the neutral form importers produce (ADR-0008, CLAUDE.md §16.2).
# Depends on base only: not on timeline, project or importers.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
oma_library(project-ir SOURCES ${SOURCES} DEPS base)
