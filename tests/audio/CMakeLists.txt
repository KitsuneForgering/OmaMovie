# libs/audio tests: the sample ring, the master clock and the outputs (the PipeWire case skips
# when no daemon answers, as in CI).
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(audio_tests SOURCES ${SOURCES} LIBS audio base)
