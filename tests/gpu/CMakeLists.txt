# libs/gpu tests. They skip (and pass) on machines without a Vulkan driver; CI installs lavapipe.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(gpu_tests SOURCES ${SOURCES} LIBS gpu base)
