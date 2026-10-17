# libs/timeline tests: model invariants, every edit with undo/redo, and per-instant evaluation.
# No GPU, media files or Qt.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(timeline_tests SOURCES ${SOURCES} LIBS timeline base)
