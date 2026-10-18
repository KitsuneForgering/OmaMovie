# libs/project-ir tests: IR validation. No media, GPU or Qt.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(project_ir_tests SOURCES ${SOURCES} LIBS project-ir base)
