# libs/playback tests: the timeline audio mix against generated fixtures (no audio device).
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(playback_tests SOURCES ${SOURCES} LIBS playback timeline audio media base)
add_dependencies(playback_tests fixtures)
