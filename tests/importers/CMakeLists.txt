# libs/importers tests: OTIO import into ProjectIR from small hand-written files. No media or Qt.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(importers_tests SOURCES ${SOURCES} LIBS importers project-ir base)
target_include_directories(importers_tests PRIVATE ${CMAKE_SOURCE_DIR}/libs/importers/src) # the XML reader is internal
