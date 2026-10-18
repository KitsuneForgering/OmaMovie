# libs/ingest — how files enter a project: a probed file as a timeline media entry, and an external
# timeline converted with its media probed. Shared by the app and tools/oma-project, so both build
# the same document from the same file. No Qt.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
oma_library(ingest SOURCES ${SOURCES} DEPS project importers project-ir timeline media base)
