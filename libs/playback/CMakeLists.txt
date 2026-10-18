# libs/playback — what playback renders from a timeline snapshot: the sequence's audio mix
# (decoded with libs/media, mixed with libs/audio). No Qt, so it is testable without a UI.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
oma_library(playback SOURCES ${SOURCES} DEPS timeline audio media base)
