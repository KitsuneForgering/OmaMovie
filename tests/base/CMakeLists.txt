# libs/base tests (one binary, one Cest suite per file).
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(base_tests SOURCES ${SOURCES} LIBS base)
