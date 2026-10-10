# libs/compositor tests: geometry and color math, the CPU reference, and GPU-vs-CPU comparisons
# (GPU cases skip without a Vulkan driver; CI runs them on lavapipe).
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(compositor_tests SOURCES ${SOURCES} LIBS compositor media gpu base)
add_dependencies(compositor_tests fixtures)
